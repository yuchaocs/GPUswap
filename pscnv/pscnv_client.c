#include "pscnv_client.h"
#include "pscnv_chan.h"

#include <linux/kthread.h>

struct pscnv_client_work {
	struct list_head entry;
	client_workfunc_t func;
	void *data;
};

/* pscnv_client_work structures are short lived and small, we use the
   slab allocator here */
static struct kmem_cache *client_work_cache = NULL;

static void
pscnv_client_pause_all_channels_of_client(struct pscnv_client *cl)
{
	struct drm_device *dev = cl->dev;
	struct pscnv_chan *ch;
	int res;
	
	list_for_each_entry(ch, &cl->channels, client_list) {
		pscnv_chan_ref(ch);
		res = pscnv_chan_pause(ch);
		if (res && res != -EALREADY) {
			NV_ERROR(dev, "pscnv_chan_pause returned %d on "
				"channel %d\n", res, ch->cid);	
		}
		res = pscnv_chan_pause_wait(ch);
		if (res) {
			NV_ERROR(dev, "pscnv_chan_pause_wait returned %d"
				" on channel %d\n", res, ch->cid);
		}
	}
}

static void
pscnv_client_continue_all_channels_of_client(struct pscnv_client *cl)
{
	struct drm_device *dev = cl->dev;
	struct pscnv_chan *ch;
	int res;
	
	list_for_each_entry(ch, &cl->channels, client_list) {
		res = pscnv_chan_continue(ch);
		if (res) {
			NV_INFO(dev, "pscnv_chan_continue returned %d on "
				"channel %d\n", res, ch->cid);
		}
		pscnv_chan_unref(ch);
	}
	
}

static int
pscnv_client_pause_thread(void *data)
{
	struct drm_device *dev = data;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct pscnv_clients *clients = dev_priv->clients;
	
	struct pscnv_client *cl;
	struct pscnv_client *cl_to_pause = NULL;
	
	if (pscnv_pause_debug >= 2) {
		NV_INFO(dev, "pscnv_client_pause_thread: init\n");
	}
	
	while (!kthread_should_stop() && !clients->pause_thread_stop) {
		if (down_trylock(&clients->need_pause)) {
			if (pscnv_pause_debug >= 2) {
				NV_INFO(dev, "pscnv_client_pause_thread: sleep\n");
			}
			down_interruptible(&clients->need_pause);
			if (pscnv_pause_debug >= 2) {
				NV_INFO(dev, "pscnv_client_pause_thread: wakeup\n");
			}
			if (kthread_should_stop() || clients->pause_thread_stop) {
				break;
			}
		}
		
		mutex_lock(&clients->lock);
		
		list_for_each_entry(cl, &clients->list, clients) {
			if (list_empty(&cl->on_empty_fifo)) {
				continue;
			} else {
				cl_to_pause = cl;
				break;
			}
		}
		
		mutex_unlock(&clients->lock);
		
		if (!cl_to_pause) {
			/* this happens, if two process add work to the
			 * on_empty_fifo list, but this thread handles them in
			 * one run */
			continue;
		}
		
		/* TODO: proper error handling in case pausing fails */
		pscnv_client_pause_all_channels_of_client(cl_to_pause);
		
		pscnv_client_run_empty_fifo_work(cl_to_pause);
		
		pscnv_client_continue_all_channels_of_client(cl_to_pause);
		
		cl_to_pause = NULL;
	}
	
	if (pscnv_pause_debug >= 2) {
		NV_INFO(dev, "pscnv_client_pause_thread: shutdown\n");
	}
	return 0;
}

static void
pscnv_client_work_ctor(void *data)
{
	struct pscnv_client_work *work = data;
	
	INIT_LIST_HEAD(&work->entry);
}

int
pscnv_clients_init(struct drm_device *dev)
{
	struct pscnv_clients *clients;
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	if (dev_priv->clients) {
		NV_INFO(dev, "Clients: already initialized!\n");
		return -EINVAL;
	}
	
	clients = kzalloc(sizeof(struct pscnv_clients), GFP_KERNEL);
	if (!clients) {
		NV_INFO(dev, "Clients: out of memory\n");
		return -ENOMEM;
	}
	
	clients->dev = dev;
	INIT_LIST_HEAD(&clients->list);
	INIT_LIST_HEAD(&clients->list_dead);
	INIT_LIST_HEAD(&clients->time_trackings);
	mutex_init(&clients->lock);
	
	if (!client_work_cache) {
		client_work_cache = kmem_cache_create("pscnv_client_work",
			sizeof(struct pscnv_client_work), 0 /* offset */, 0, /* flags */
			pscnv_client_work_ctor);
	}
	
	if (!client_work_cache) {
		NV_INFO(dev, "Clients: failed to init client_work cache\n");
		kfree(clients);
		return -ENOMEM;
	}
	
	sema_init(&clients->need_pause, 0);
	clients->pause_thread = kthread_run(pscnv_client_pause_thread, dev, "pscnv_pause");
	
	if (IS_ERR_OR_NULL(clients->pause_thread)) {
		NV_INFO(dev, "Clients: failed to start pause thread\n");
		kfree(clients);
		return -ENOMEM;
	}
	
	dev_priv->clients = clients;
	return 0;
}

int
pscnv_clients_exit(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct pscnv_clients *clients = dev_priv->clients;
	struct pscnv_client_timetrack *tt, *tt_tmp;
	struct pscnv_client *cl, *cl_tmp;
	
	BUG_ON(!clients);

	WARN_ON(!mutex_trylock(&clients->lock));
	
	WARN_ON(!list_empty(&clients->list));
	list_for_each_entry_safe(cl, cl_tmp, &clients->list, clients) {
		list_del(&cl->clients);
		kfree(cl);
	}
	
	list_for_each_entry_safe(tt, tt_tmp, &clients->time_trackings, list) {
		list_del(&tt->list);
		kfree(tt);
	}
	
	list_for_each_entry_safe(cl, cl_tmp, &clients->list_dead, clients) {
		list_del(&cl->clients);
		kfree(cl);
	}
	
	WARN_ON(!clients->pause_thread);
	if (clients->pause_thread) {
		clients->pause_thread_stop = true;
		wmb();
		up(&clients->need_pause);
		
		kthread_stop(clients->pause_thread);
	}
	if (client_work_cache) {
		/* warning indicates that work_cache is not empty */
		WARN_ON(kmem_cache_shrink(client_work_cache));
	
		kmem_cache_destroy(client_work_cache);
	}
	
	kfree(clients);
	dev_priv->clients = NULL;
	
	return 0;
}

static struct pscnv_client*
pscnv_client_search_pid_unlocked(struct drm_device *dev, pid_t pid)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct pscnv_client *cur;
	
	BUG_ON(!dev_priv->clients);
	BUG_ON(!mutex_is_locked(&dev_priv->clients->lock));
	
	list_for_each_entry(cur, &dev_priv->clients->list, clients) {
		if (cur->pid == pid) {
			return cur;
		}
	}
	
	return NULL;
}

/* get the client instance for the current process, or NULL */
struct pscnv_client*
pscnv_client_search_pid(struct drm_device *dev, pid_t pid)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct pscnv_client *cl;
	
	mutex_lock(&dev_priv->clients->lock);
	cl = pscnv_client_search_pid_unlocked(dev, pid);
	mutex_unlock(&dev_priv->clients->lock);
	
	return cl;
}

static struct pscnv_client*
pscnv_client_new_unlocked(struct drm_device *dev, pid_t pid, const char *comm)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct pscnv_client *new;
	
	BUG_ON(!dev_priv->clients);
	BUG_ON(!mutex_is_locked(&dev_priv->clients->lock));
	
	if (pscnv_client_search_pid_unlocked(dev, pid) != NULL) {
		NV_ERROR(dev, "pscnv_client_new: client with pid %d already "
			      " in list\n", pid);
		return NULL;
	}
	
	new = kzalloc(sizeof(struct pscnv_client), GFP_KERNEL);
	if (!new) {
		NV_ERROR(dev, "pscnv_client_new: out of memory\n");
		return NULL;
	}
	
	new->dev = dev;
	new->pid = pid;
	kref_init(&new->ref);
	INIT_LIST_HEAD(&new->clients);
	INIT_LIST_HEAD(&new->channels);
	INIT_LIST_HEAD(&new->on_empty_fifo);
	pscnv_chunk_list_init(&new->swapping_options);
	pscnv_chunk_list_init(&new->already_swapped);
	pscnv_chunk_list_init(&new->swap_pending);
	strncpy(new->comm, comm, TASK_COMM_LEN-1);
	
	list_add_tail(&new->clients, &dev_priv->clients->list);
	
	BUG_ON(!pscnv_client_search_pid_unlocked(dev, pid));
	
	return new;
}

static void
pscnv_client_free_unlocked(struct pscnv_client *cl)
{
	struct drm_device *dev = cl->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	
	BUG_ON(!dev_priv->clients);
	BUG_ON(!mutex_is_locked(&dev_priv->clients->lock));
	
	/* remove from clients->list */
	list_del_init(&cl->clients);
	
	
	WARN_ON(!pscnv_chunk_list_empty(&cl->swapping_options));
	pscnv_chunk_list_free(&cl->swapping_options);
	WARN_ON(!pscnv_chunk_list_empty(&cl->already_swapped));
	pscnv_chunk_list_free(&cl->already_swapped);
	WARN_ON(!pscnv_chunk_list_empty(&cl->swap_pending));
	pscnv_chunk_list_free(&cl->swap_pending);
	
	/* keep the client data structure around, so we can read its time trackings */
	list_add_tail(&cl->clients, &dev_priv->clients->list_dead);
}
	

/* remove the client from the clients list and free memory */
void
pscnv_client_ref_free(struct kref *ref)
{
	struct pscnv_client *cl = container_of(ref, struct pscnv_client, ref);
	struct drm_device *dev = cl->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	char size_str[16];
	
	uint64_t vram_usage = atomic64_read(&cl->vram_usage);
	uint64_t vram_swapped = atomic64_read(&cl->vram_swapped);
	uint64_t vram_demand = atomic64_read(&cl->vram_demand);

	pscnv_mem_human_readable(size_str, cl->vram_max);
	NV_INFO(dev, "closing client %s:%d (vram_max=%s)\n",
			cl->comm, cl->pid, size_str);
	
	if (vram_usage > 0 || vram_swapped > 0 || vram_demand > 0) {
		NV_ERROR(dev, "client %s:%d still has vram_usage=%llu, "
			"vram_swapped=%llu, vram_demand=%llu at exit\n",
			cl->comm, cl->pid, vram_usage, vram_swapped, vram_demand);
	}
	
	mutex_lock(&dev_priv->clients->lock);
	pscnv_client_free_unlocked(cl);
	mutex_unlock(&dev_priv->clients->lock);
}

int
pscnv_client_open(struct drm_device *dev, struct drm_file *file_priv)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct pscnv_client *cl;
	const pid_t pid = file_priv->pid;
	struct task_struct *task = pid_task(find_vpid(pid), PIDTYPE_PID);
	
	if (!task) {
		NV_ERROR(dev, "pscnv_client_new: no process for pid %d\n", pid);
		return 0;
	}
	
	mutex_lock(&dev_priv->clients->lock);
	
	cl = pscnv_client_search_pid_unlocked(dev, pid);
	if (cl) {
		pscnv_client_ref(cl);
	} else {
		NV_INFO(dev, "new client %s:%d\n", task->comm, pid);
		cl = pscnv_client_new_unlocked(dev, pid, task->comm);
	}
	
	mutex_unlock(&dev_priv->clients->lock);
	
	if (!cl) {
		NV_ERROR(dev, "pscnv_client_open: failed for pid %d\n", pid);
		return -EINVAL;
	}
	
	return 0;
}

void
pscnv_client_postclose(struct drm_device *dev, struct drm_file *file_priv)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct pscnv_client *cl;
	const pid_t pid = file_priv->pid;
	
	mutex_lock(&dev_priv->clients->lock);
	cl = pscnv_client_search_pid_unlocked(dev, pid);
	mutex_unlock(&dev_priv->clients->lock);
	
	if (!cl) {
		NV_ERROR(dev, "pscnv_client_postclose: pid %d not in list\n", pid);
		return;
	}

	/* must be called without lock! */
	pscnv_client_unref(cl);
}

void
pscnv_client_do_on_empty_fifo_unlocked(struct pscnv_client *cl, client_workfunc_t func, void *data)
{
	struct drm_device *dev = cl->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	
	struct pscnv_client_work *work;
	bool need_pause = list_empty(&cl->on_empty_fifo);
	
	work = kmem_cache_alloc(client_work_cache, GFP_KERNEL);
	BUG_ON(!work);
	
	work->func = func;
	work->data = data;
	
	list_add_tail(&work->entry, &cl->on_empty_fifo);
	
	if (need_pause) {
		up(&dev_priv->clients->need_pause);
	}
}

void
pscnv_client_run_empty_fifo_work(struct pscnv_client *cl)
{
	struct drm_device *dev = cl->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct pscnv_client_work *work;
	
	mutex_lock(&dev_priv->clients->lock);
	while (!list_empty(&cl->on_empty_fifo)) {
		/* the work in the queue meight take some time, so we always
		   pick just one element and then release the lock again.
		   this also allows others to queue additional work. */
		
		work = list_first_entry(&cl->on_empty_fifo, struct pscnv_client_work, entry);
		/* use del_init here, so that the work may be safely  reused */
		list_del_init(&work->entry);
		
		mutex_unlock(&dev_priv->clients->lock);
		
		/* release lock here, because this may take some time */
		work->func(work->data, cl);
		kmem_cache_free(client_work_cache, work);
		
		mutex_lock(&dev_priv->clients->lock);
	} /* ^^ list_empty checked again */
	
	mutex_unlock(&dev_priv->clients->lock);
}

uint64_t
pscnv_clients_vram_common_unlocked(struct drm_device *dev, size_t offset)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct pscnv_clients *clients = dev_priv->clients;
	struct pscnv_client *cur;
	
	uint64_t sum = 0;
	
	list_for_each_entry(cur, &clients->list, clients) {
		sum += atomic64_read((atomic64_t*)((char*)cur + offset));
	}
	
	return sum;
}

uint64_t
pscnv_clients_vram_common(struct drm_device *dev, size_t offset)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct pscnv_clients *clients = dev_priv->clients;
	uint64_t res;
	
	mutex_lock(&clients->lock);
	res = pscnv_clients_vram_common_unlocked(dev, offset);
	mutex_unlock(&dev_priv->clients->lock);
	
	return res;
}

/* safe for cl == NULL */
void
pscnv_client_track_time(struct pscnv_client *cl, s64 start, s64 duration, u64 bytes, const char *name)
{
	struct drm_device *dev;
	struct drm_nouveau_private *dev_priv;
	struct pscnv_client_timetrack *tt;
	
	if (!cl)
		return;
	
	dev = cl->dev;
	dev_priv = dev->dev_private;
	
	BUG_ON(!dev_priv->clients);
	
	tt = kzalloc(sizeof(struct pscnv_client_timetrack), GFP_KERNEL);
	if (!tt) {
		return;
	}
	
	INIT_LIST_HEAD(&tt->list);
	tt->client = cl;
	tt->type = name;
	tt->start = start;
	tt->duration = duration;
	tt->bytes = bytes;
	list_add_tail(&tt->list, &dev_priv->clients->time_trackings);
}

#include "drm.h"
#include "nouveau_drv.h"
#include "pscnv_mem.h"

#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/gfp.h>

static int
pscnv_sysram_vm_fault(struct pscnv_bo *bo, struct vm_area_struct *vma, struct vm_fault *vmf)
{
	uint64_t offset = (uint64_t)vmf->virtual_address - vma->vm_start;
	struct page *res;

	res = bo->pages[offset >> PAGE_SHIFT];
	get_page(res);
	vmf->page = res;
	return 0;
}

int
pscnv_sysram_alloc_chunk(struct pscnv_chunk *cnk)
{
	struct pscnv_bo *bo = pscnv_chunk_bo(cnk);
	struct drm_device *dev = bo->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	
	int ret;
	uint64_t size = pscnv_chunk_size(cnk);
	int numpages = size >> PAGE_SHIFT;
	int i, j;
	
	if (pscnv_chunk_expect_alloc_type(cnk, PSCNV_CHUNK_UNALLOCATED,
						"pscnv_sysram_alloc_chunk") {
		return -EINVAL;
	}
	
	WARN_ON(cnk->pages);
	
	cnk->pages = kzalloc(numpages * sizeof(struct pscnv_page_and_dma), GFP_KERNEL);
	
	for (i = 0; i < numpages; i++) {
		cnk->pages[i].k = alloc_pages(dev_priv->dma_mask > 0xffffffff ? GFP_DMA32 : GFP_KERNEL, 0);
		if (!cnk->pages[i].k) {
			for (j = 0; j < i; j++)
				put_page(cnk->pages[j].k);
			kfree(cnk->pages);
			cnk->pages = NULL;
			return -ENOMEM;
		}
	}
	
	for (i = 0; i < numpages; i++) {
		cnk->pages[i].dma = pci_map_page(bo->dev->pdev, cnk->pages[i].k, 0, PAGE_SIZE, PCI_DMA_BIDIRECTIONAL);
		if (pci_dma_mapping_error(bo->dev->pdev, cnk->pages[i].dma)) {
			for (j = 0; j < i; j++)
				pci_unmap_page(bo->dev->pdev, bo->pages[j].dma, PAGE_SIZE, PCI_DMA_BIDIRECTIONAL);
			for (j = 0; j < numpages; j++)
				put_page(bo->pages[j].k);
			kfree(cnk->pages);
			cnk->pages = NULL;
			return -ENOMEM;
		}
	}
	
	cnk->alloc_type = PSCNV_CHUNK_SYSRAM;
	
	return 0;
}

int
pscnv_sysram_alloc(struct pscnv_bo *bo)
{
	struct drm_nouveau_private *dev_priv = bo->dev->dev_private;
	
	int ret = 0;
	uint32_t i;
	
	for (i = 0; i < bo->n_chunks; i++) {
		ret = pscnv_sysram_alloc_chunk(bo->chunks[i]);
		if (ret) {
			for (i--; i >= 0; i--)
				pscnv_sysram_free_chunk(bo->chunks[i]);
				i--;
			}
			break;
		}
	}
	
	if (ret) {
		return ret;
	}
	
	if (!bo->vm_fault) {
		/* we should be the first to touch this BO anyways, but we
		 * definetly don't want to overwrite a more specific fault
		 * handler */
		bo->vm_fault = pscnv_sysram_vm_fault;
	}
	
	return 0;
}

void
pscnv_sysram_free_chunk(struct pscnv_chunk *cnk)
{
	struct pscnv_bo *bo = pscnv_chunk_bo(cnk);
	struct drm_device *dev = bo->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	
	uint64_t size = pscnv_chunk_size(cnk);
	int numpages = size >> PAGE_SHIFT;
	int i;
	
	for (i = 0; i < numpages; i++)
		pci_unmap_page(bo->dev->pdev, cnk->pages[i].dma, PAGE_SIZE, PCI_DMA_BIDIRECTIONAL);
	for (i = 0; i < numpages; i++)
		put_page(cnk->pages[i].k);
	
	kfree(cnk->pages);
	cnk->pages = NULL;
	
	cnk->alloc_type = PSCNV_CHUNK_UNALLOCATED;
}

uint32_t
nv_rv32_sysram(struct pscnv_bo *bo, unsigned offset)
{
	uint32_t *mem;
	uint32_t val;

	mem = kmap_atomic(bo->pages[offset >> PAGE_SHIFT], KM_USER0);
	val = mem[(offset & 0xfff) >> 2];
	kunmap_atomic(mem, KM_USER0);

	return val;
}

void
nv_wv32_sysram(struct pscnv_bo *bo, unsigned offset, uint32_t val)
{
	uint32_t *mem;
	
	mem = kmap_atomic(bo->pages[offset >> PAGE_SHIFT], KM_USER0);
	mem[(offset & 0xfff) >> 2] = val;
	kunmap_atomic(mem, KM_USER0);
}

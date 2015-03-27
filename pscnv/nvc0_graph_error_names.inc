/*******************************************************************************
 * ERROR NAMES
 * (to be included in nvc0_graph.c)
 ******************************************************************************/

static const struct nouveau_enum nve0_sked_error[] = {
	{ 7, "CONSTANT_BUFFER_SIZE" },
	{ 9, "LOCAL_MEMORY_SIZE_POS" },
	{ 10, "LOCAL_MEMORY_SIZE_NEG" },
	{ 11, "WARP_CSTACK_SIZE" },
	{ 12, "TOTAL_TEMP_SIZE" },
	{ 13, "REGISTER_COUNT" },
	{ 18, "TOTAL_THREADS" },
	{ 20, "PROGRAM_OFFSET" },
	{ 21, "SHARED_MEMORY_SIZE" },
	{ 25, "SHARED_CONFIG_TOO_SMALL" },
	{ 26, "TOTAL_REGISTER_COUNT" },
	{}
};

static const struct nouveau_enum nvc0_gpc_rop_error[] = {
	{ 1, "RT_PITCH_OVERRUN" },
	{ 4, "RT_WIDTH_OVERRUN" },
	{ 5, "RT_HEIGHT_OVERRUN" },
	{ 7, "ZETA_STORAGE_TYPE_MISMATCH" },
	{ 8, "RT_STORAGE_TYPE_MISMATCH" },
	{ 10, "RT_LINEAR_MISMATCH" },
	{}
};

static const struct nouveau_enum nvc0_mp_warp_error[] = {
	{ 0x00, "NO_ERROR" },
	{ 0x01, "STACK_MISMATCH" },
	{ 0x05, "MISALIGNED_PC" },
	{ 0x08, "MISALIGNED_GPR" },
	{ 0x09, "INVALID_OPCODE" },
	{ 0x0d, "GPR_OUT_OF_BOUNDS" },
	{ 0x0e, "MEM_OUT_OF_BOUNDS" },
	{ 0x0f, "UNALIGNED_MEM_ACCESS" },
	{ 0x11, "INVALID_PARAM" },
	{}
};

static const struct nouveau_bitfield nvc0_mp_global_error[] = {
	{ 0x00000004, "MULTIPLE_WARP_ERRORS" },
	{ 0x00000008, "OUT_OF_STACK_SPACE" },
	{}
};

/* There must be a *lot* of these. Will take some time to gather them up. */
const struct nouveau_enum nv50_data_error_names[] = {
	{ 0x00000003, "INVALID_OPERATION", NULL },
	{ 0x00000004, "INVALID_VALUE", NULL },
	{ 0x00000005, "INVALID_ENUM", NULL },
	{ 0x00000008, "INVALID_OBJECT", NULL },
	{ 0x00000009, "READ_ONLY_OBJECT", NULL },
	{ 0x0000000a, "SUPERVISOR_OBJECT", NULL },
	{ 0x0000000b, "INVALID_ADDRESS_ALIGNMENT", NULL },
	{ 0x0000000c, "INVALID_BITFIELD", NULL },
	{ 0x0000000d, "BEGIN_END_ACTIVE", NULL },
	{ 0x0000000e, "SEMANTIC_COLOR_BACK_OVER_LIMIT", NULL },
	{ 0x0000000f, "VIEWPORT_ID_NEEDS_GP", NULL },
	{ 0x00000010, "RT_DOUBLE_BIND", NULL },
	{ 0x00000011, "RT_TYPES_MISMATCH", NULL },
	{ 0x00000012, "RT_LINEAR_WITH_ZETA", NULL },
	{ 0x00000015, "FP_TOO_FEW_REGS", NULL },
	{ 0x00000016, "ZETA_FORMAT_CSAA_MISMATCH", NULL },
	{ 0x00000017, "RT_LINEAR_WITH_MSAA", NULL },
	{ 0x00000018, "FP_INTERPOLANT_START_OVER_LIMIT", NULL },
	{ 0x00000019, "SEMANTIC_LAYER_OVER_LIMIT", NULL },
	{ 0x0000001a, "RT_INVALID_ALIGNMENT", NULL },
	{ 0x0000001b, "SAMPLER_OVER_LIMIT", NULL },
	{ 0x0000001c, "TEXTURE_OVER_LIMIT", NULL },
	{ 0x0000001e, "GP_TOO_MANY_OUTPUTS", NULL },
	{ 0x0000001f, "RT_BPP128_WITH_MS8", NULL },
	{ 0x00000021, "Z_OUT_OF_BOUNDS", NULL },
	{ 0x00000023, "XY_OUT_OF_BOUNDS", NULL },
	{ 0x00000024, "VP_ZERO_INPUTS", NULL },
	{ 0x00000027, "CP_MORE_PARAMS_THAN_SHARED", NULL },
	{ 0x00000028, "CP_NO_REG_SPACE_STRIPED", NULL },
	{ 0x00000029, "CP_NO_REG_SPACE_PACKED", NULL },
	{ 0x0000002a, "CP_NOT_ENOUGH_WARPS", NULL },
	{ 0x0000002b, "CP_BLOCK_SIZE_MISMATCH", NULL },
	{ 0x0000002c, "CP_NOT_ENOUGH_LOCAL_WARPS", NULL },
	{ 0x0000002d, "CP_NOT_ENOUGH_STACK_WARPS", NULL },
	{ 0x0000002e, "CP_NO_BLOCKDIM_LATCH", NULL },
	{ 0x00000031, "ENG2D_FORMAT_MISMATCH", NULL },
	{ 0x0000003f, "PRIMITIVE_ID_NEEDS_GP", NULL },
	{ 0x00000044, "SEMANTIC_VIEWPORT_OVER_LIMIT", NULL },
	{ 0x00000045, "SEMANTIC_COLOR_FRONT_OVER_LIMIT", NULL },
	{ 0x00000046, "LAYER_ID_NEEDS_GP", NULL },
	{ 0x00000047, "SEMANTIC_CLIP_OVER_LIMIT", NULL },
	{ 0x00000048, "SEMANTIC_PTSZ_OVER_LIMIT", NULL },
	{}
};
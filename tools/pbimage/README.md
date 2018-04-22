

/* Image header */
u32 header_magic
u32 header_version
u32 no_of_components
u32 _reserved[16]
u8[256] payload_sha256
u8[...] signature



/* Component header */
u32 comp_header_version
u32 component_type
u32 load_addr_low
u32 load_addr_high
u32 component_size
u32 component_offset
u32 _reserved[16]


 ... DATA ...




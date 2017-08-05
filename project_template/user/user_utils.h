#define EXPORT
#define INTERVAL_IMPL
#define TOOLS_API

#define PREDEF_PTR(type, id) \
                type *g_global_##id;

#define EXPORT_PTR(type, id) \
                type *g_global_##id;

#define EXPORT_PTR_V(type, id, v) \
                type *g_global_##id = v;

#define EXTERN_PTR(type, id) \
                extern type *g_global_##id;

#define IMPORT_PTR(id) \
                g_global_##id

#define IPTR(id) IMPORT_PTR(id)

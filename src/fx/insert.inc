static constexpr uint32_t flavor_uid = 0x485303;
static constexpr char flavor_name[] = "AIR Flavor Pro";
static constexpr char flavor_path[] = "MPC:AIR Flavor Pro";
static uintptr_t stock_base;
static bool native_factory_ready;

struct Callback {
    uintptr_t data[2];
    void *manager;
    void (*invoke)(Callback *, void **, char **);
};

static void destroy(void *effect)
{
    auto **methods = field<void **>(effect, 0);
    reinterpret_cast<void (*)(void *)>(methods[1])(effect);
}

static void *create_flavor(void *format, const void *description)
{
    auto lifetime = std::make_shared<std::array<uintptr_t, 3>>();
    void *effect = ::operator new(2360);
    auto ctor = reinterpret_cast<void (*)(void *, const void *, void *, void *)>(stock_base + 0x2445a90);
    ctor(effect, description, field<void *>(format, 0x20), &lifetime);
    return effect;
}

static bool might_contain(void *format, const char *const *path)
{
    if (path && *path && !strcmp(*path, flavor_path)) return true;
    return reinterpret_cast<bool (*)(void *, const char *const *)>(stock_base + 0x2536990)(format, path);
}

static void create_instance(void *format, const void *description, double rate, int block, Callback *callback)
{
    if (field<uint32_t>(const_cast<void *>(description), 0x48) != flavor_uid ||
        strcmp(field<const char *>(const_cast<void *>(description), 0x30), flavor_path)) {
        reinterpret_cast<void (*)(void *, const void *, double, int, Callback *)>(stock_base + 0x2537560)(
            format, description, rate, block, callback);
        return;
    }

    void *effect = create_flavor(format, description);
    char *error;
    auto make_string = reinterpret_cast<void (*)(char **, const char *)>(stock_base + 0xf535c0);
    auto drop_string = reinterpret_cast<void (*)(char **)>(stock_base + 0xf0a290);
    make_string(&error, "");
    fprintf(stderr, "flavor: created native instance\n");
    callback->invoke(callback, &effect, &error);
    drop_string(&error);
    if (effect) destroy(effect);
}

/* Install before the engine restores a project, which precedes the first picker
 * call. Preserve every stock entry except discovery and creation for our UID. */
__attribute__((constructor)) static void install_native_factory()
{
    dl_iterate_phdr(base_address, &stock_base);
    if (!stock_base) return;
    auto *methods = reinterpret_cast<uintptr_t *>(stock_base + 0x6eacf78);
    if (methods[5] != stock_base + 0x2536990 || methods[13] != stock_base + 0x2537560) return;
    const long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) return;
    const uintptr_t page = reinterpret_cast<uintptr_t>(methods + 5) & ~(uintptr_t(page_size) - 1);
    const size_t span = reinterpret_cast<uintptr_t>(methods + 14) - page > uintptr_t(page_size)
        ? size_t(page_size) * 2 : size_t(page_size);
    if (mprotect(reinterpret_cast<void *>(page), span, PROT_READ | PROT_WRITE)) return;
    methods[5] = reinterpret_cast<uintptr_t>(might_contain);
    methods[13] = reinterpret_cast<uintptr_t>(create_instance);
    if (mprotect(reinterpret_cast<void *>(page), span, PROT_READ)) {
        methods[5] = stock_base + 0x2536990;
        methods[13] = stock_base + 0x2537560;
        mprotect(reinterpret_cast<void *>(page), span, PROT_READ);
        return;
    }
    native_factory_ready = true;
}

static bool install()
{
    if (!native_factory_ready) return false;
    auto find_manager = reinterpret_cast<void *(*)(uintptr_t)>(dlsym(RTLD_NEXT, "context_locate_plugin_manager"));
    auto *manager = find_manager(stock_base);
    if (!manager || field<uintptr_t>(manager, 0) != stock_base + 0x6da1f90) return false;
    auto *groups = field<void *>(manager, 0x1d8);
    auto *begin = field<Category *>(groups, 0x648);
    auto *end = field<Category *>(groups, 0x650);
    if (end >= field<Category *>(groups, 0x658)) return false;
    const char *stock = nullptr;
    for (auto *category = begin; category < end; ++category)
        for (int i = 0; i < category->count; ++i) {
            auto *item = category->items + i * 96;
            if (!strcmp(field<const char *>(item, 0), "AIR Lo-Fi")) stock = item;
        }
    if (!stock) return false;

    auto make_string = reinterpret_cast<void (*)(char **, const char *)>(stock_base + 0xf535c0);
    auto copy_string = reinterpret_cast<void (*)(char **, const char **)>(stock_base + 0xf1c640);
    char *description = static_cast<char *>(malloc(96));
    if (!description) return false;
    memcpy(description, stock, 96);
    for (unsigned offset = 0; offset <= 0x30; offset += 8)
        copy_string(&field<char *>(description, offset), &field<const char *>(const_cast<char *>(stock), offset));
    auto drop_string = reinterpret_cast<void (*)(char **)>(stock_base + 0xf0a290);
    for (auto offset : {0u, 8u, 0x30u}) drop_string(&field<char *>(description, offset));
    make_string(&field<char *>(description, 0), flavor_name);
    make_string(&field<char *>(description, 8), flavor_name);
    make_string(&field<char *>(description, 0x30), flavor_path);
    field<uint32_t>(description, 0x48) = flavor_uid;
    make_string(const_cast<char **>(&end->name), "AIR Effects");
    make_string(const_cast<char **>(&end->subcategory), "");
    end->items = description;
    end->capacity = end->count = 1;

    field<Category *>(groups, 0x650) = end + 1;
    fprintf(stderr, "flavor: registered native insert\n");
    return true;
}

struct Row { uint32_t uid; int preset, group; char name[24]; };

extern "C" int fxlist_get(void *context, Row *rows, int capacity)
{
    static auto next = reinterpret_cast<int (*)(void *, Row *, int)>(dlsym(RTLD_NEXT,"fxlist_get"));
    int count = next(context, rows, capacity);
    if (!context || !rows || count <= 0 || count >= capacity) return count;
    static bool installed = false;
    if (!installed) installed = install();
    if (installed) rows[count++] = {flavor_uid, -1, 4, "AIR Flavor Pro"};
    return count;
}

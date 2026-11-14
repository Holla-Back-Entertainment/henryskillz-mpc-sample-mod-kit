/* Opens every HS effect's editor the way a CLAP host does (create, set_parent into a real window, show),
 * drags its first knob up with real mouse events, then checks the parameter moved and that the plugin
 * sent gesture-begin, value and gesture-end to the host. Writes a PNG of each editor to a folder.
 *   clang -fobjc-arc -I$CLAP_SDK/include check-gui-mac.m -framework Cocoa -o check-gui
 *   ./check-gui "HS Effects.clap" out-folder */
#import <Cocoa/Cocoa.h>
#include <clap/clap.h>
#include <dlfcn.h>
#include <assert.h>

static int begins, values, ends, flushes;
static clap_id moved = CLAP_INVALID_ID;
static const clap_plugin_t *plug;

static const void *h_ext(const clap_host_t *h, const char *id);
static void h_noop(const clap_host_t *h) { (void)h; }
static void h_rescan(const clap_host_t *h, clap_param_rescan_flags f) { (void)h; (void)f; }
static void h_clear(const clap_host_t *h, clap_id id, clap_param_clear_flags f) { (void)h; (void)id; (void)f; }
static void h_flush_req(const clap_host_t *h) { (void)h; ++flushes; }
static const clap_host_params_t host_params = {h_rescan, h_clear, h_flush_req};
static const clap_host_t host = {CLAP_VERSION_INIT, NULL, "check-gui", "Henry Skillz", "", "1", h_ext, h_noop, h_noop, h_noop};
static const void *h_ext(const clap_host_t *h, const char *id) { (void)h; return strcmp(id, CLAP_EXT_PARAMS) ? NULL : &host_params; }

static uint32_t in_size(const clap_input_events_t *l) { (void)l; return 0; }
static const clap_event_header_t *in_get(const clap_input_events_t *l, uint32_t i) { (void)l; (void)i; return NULL; }
static bool out_push(const clap_output_events_t *l, const clap_event_header_t *e)
{
    (void)l;
    begins += e->type == CLAP_EVENT_PARAM_GESTURE_BEGIN;
    values += e->type == CLAP_EVENT_PARAM_VALUE;
    if (e->type == CLAP_EVENT_PARAM_VALUE) moved = ((const clap_event_param_value_t *)e)->param_id;
    ends += e->type == CLAP_EVENT_PARAM_GESTURE_END;
    return true;
}

int main(int argc, char **argv)
{
    @autoreleasepool {
        if (argc < 3) return 2;
        NSString *bin = [NSString stringWithFormat:@"%s/Contents/MacOS/HS Effects", argv[1]];
        void *lib = dlopen(bin.UTF8String, RTLD_LOCAL | RTLD_NOW);
        assert(lib);
        const clap_plugin_entry_t *entry = dlsym(lib, "clap_entry");
        assert(entry && entry->init(argv[1]));
        const clap_plugin_factory_t *f = entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
        int fails = 0;
        for (uint32_t i = 0; i < f->get_plugin_count(f); ++i) {
            const clap_plugin_descriptor_t *d = f->get_plugin_descriptor(f, i);
            plug = f->create_plugin(f, &host, d->id);
            assert(plug && plug->init(plug) && plug->activate(plug, 48000, 32, 512));
            const clap_plugin_gui_t *gui = plug->get_extension(plug, CLAP_EXT_GUI);
            const clap_plugin_params_t *params = plug->get_extension(plug, CLAP_EXT_PARAMS);
            assert(gui && gui->is_api_supported(plug, CLAP_WINDOW_API_COCOA, false));
            assert(gui->create(plug, CLAP_WINDOW_API_COCOA, false));
            uint32_t w, h;
            assert(gui->get_size(plug, &w, &h) && w == 960 && h == 600);
            NSWindow *win = [[NSWindow alloc] initWithContentRect:NSMakeRect(100, 100, w, h)
                                                        styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
            win.releasedWhenClosed = NO;
            clap_window_t cw = {CLAP_WINDOW_API_COCOA, {.cocoa = (__bridge void *)win.contentView}};
            assert(gui->set_parent(plug, &cw) && gui->show(plug));
            [win orderFront:nil];
            NSView *v = win.contentView.subviews.firstObject;
            assert(v);

            /* the first knob of page 1 sits at panel (57, 155) for every effect except Para EQ (boxes at y 180) */
            CGFloat y = !strcmp(d->name, "HS Para EQ") ? 180 * 3 : 155 * 3, x = 57 * 3;
            double before[512] = {0}, after = 0;
            uint32_t n = params->count(plug);
            for (uint32_t k = 0; k < n && k < 512; ++k) {
                clap_param_info_t info;
                params->get_info(plug, k, &info);
                params->get_value(plug, info.id, &before[info.id < 512 ? info.id : 0]);
            }
            NSPoint at = [v convertPoint:NSMakePoint(x, y) toView:nil];
            /* two drags, up 40 then down 20: moves a knob that starts at either end of its range */
            NSEventType types[] = {NSEventTypeLeftMouseDown, NSEventTypeLeftMouseDragged, NSEventTypeLeftMouseUp};
            static const CGFloat dy[2][3] = {{0, 40, 40}, {0, -20, -20}};   /* window coords: up is +y */
            for (int g = 0; g < 2; ++g)
                for (int s = 0; s < 3; ++s) {
                    NSPoint p = NSMakePoint(at.x, at.y + dy[g][s]);
                    NSEvent *e = [NSEvent mouseEventWithType:types[s] location:p modifierFlags:0 timestamp:0
                                                windowNumber:win.windowNumber context:nil eventNumber:0 clickCount:1 pressure:1];
                    [win sendEvent:e];
                }
            begins = values = ends = 0;
            moved = CLAP_INVALID_ID;
            clap_input_events_t in = {NULL, in_size, in_get};
            clap_output_events_t out = {NULL, out_push};
            params->flush(plug, &in, &out);
            int ok = moved < 512 && params->get_value(plug, moved, &after) && after != before[moved]
                     && begins == 2 && values == 2 && ends == 2;
            fails += !ok;
            printf("%s %-18s port %3u %.2f -> %.2f, events %d/%d/%d\n", ok ? "PASS" : "FAIL", d->name, moved, moved < 512 ? before[moved] : 0, after, begins, values, ends);

            [v display];
            NSBitmapImageRep *rep = [v bitmapImageRepForCachingDisplayInRect:v.bounds];
            [v cacheDisplayInRect:v.bounds toBitmapImageRep:rep];
            NSString *png = [NSString stringWithFormat:@"%s/%@.png", argv[2], [@(d->name) stringByReplacingOccurrencesOfString:@" " withString:@"_"]];
            [[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:png atomically:YES];

            gui->hide(plug);
            gui->destroy(plug);
            assert(v.superview == nil);
            [win close];
            plug->deactivate(plug);
            plug->destroy(plug);
        }
        printf("%u editors, %d fails\n", f->get_plugin_count(f), fails);
        entry->deinit();
        return fails != 0;
    }
}

/* clap.gui on macOS: an NSView showing the MPC faceplate (hs_clap_panel.c) at 3x, nearest-neighbour,
 * so it is the same pixels as the MPC Sample's screen. Drag a knob up/down (or scroll on it) to turn it;
 * the arrows in the header page through the effect, as B2 does on the MPC. Embedded only, no floating window.
 * ponytail: redraws on a 25 Hz timer (meters and host automation); the panel is 320x200, so it costs little. */
#import <Cocoa/Cocoa.h>
#include "hs_clap_gui.h"
#include "hs_clap_panel.h"

enum { SCALE = 3 };

@interface HSPanelView : NSView {
@public
    hs_panel *panel;
    hs_gui_target target;
    NSTimer *timer;
    int dragPort;
    float dragStart;
    CGFloat dragFromY;
}
@end

@implementation HSPanelView
- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent *)event { (void)event; return YES; }

- (void)drawRect:(NSRect)dirty
{
    (void)dirty;
    const uint32_t *px = hs_panel_draw(panel, target.handle(target.ctx));
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGDataProviderRef data = CGDataProviderCreateWithData(NULL, px, HS_PANEL_W * HS_PANEL_H * 4, NULL);
    CGImageRef image = CGImageCreate(HS_PANEL_W, HS_PANEL_H, 8, 32, HS_PANEL_W * 4, space,
                                     kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst, data, NULL, false,
                                     kCGRenderingIntentDefault);
    CGContextRef c = [[NSGraphicsContext currentContext] CGContext];
    CGContextSaveGState(c);
    CGContextSetInterpolationQuality(c, kCGInterpolationNone);
    CGContextTranslateCTM(c, 0, self.bounds.size.height);   /* the view is flipped; images draw bottom-up */
    CGContextScaleCTM(c, 1, -1);
    CGContextDrawImage(c, self.bounds, image);
    CGContextRestoreGState(c);
    CGImageRelease(image);
    CGDataProviderRelease(data);
    CGColorSpaceRelease(space);
}

- (NSPoint)panelPoint:(NSEvent *)event
{
    NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
    return NSMakePoint(p.x * HS_PANEL_W / self.bounds.size.width, p.y * HS_PANEL_H / self.bounds.size.height);
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint p = [self panelPoint:event];
    dragPort = hs_panel_knob_at(panel, (int)p.x, (int)p.y);
    if (dragPort < 0) {
        if (hs_panel_click(panel, (int)p.x, (int)p.y)) [self setNeedsDisplay:YES];
        return;
    }
    dragStart = target.ctl[dragPort];
    dragFromY = p.y;
    target.edit(target.ctx, HS_EDIT_BEGIN, (uint32_t)dragPort, dragStart);
}

- (void)mouseDragged:(NSEvent *)event
{
    if (dragPort < 0) return;
    NSPoint p = [self panelPoint:event];
    float fine = (event.modifierFlags & NSEventModifierFlagShift) ? .2f : 1;   /* shift: fine adjust */
    float v = hs_panel_drag(panel, dragPort, dragStart, (float)(dragFromY - p.y) * fine);
    target.edit(target.ctx, HS_EDIT_VALUE, (uint32_t)dragPort, v);
    [self setNeedsDisplay:YES];
}

- (void)mouseUp:(NSEvent *)event
{
    (void)event;
    if (dragPort >= 0) target.edit(target.ctx, HS_EDIT_END, (uint32_t)dragPort, target.ctl[dragPort]);
    dragPort = -1;
}

- (void)scrollWheel:(NSEvent *)event
{
    NSPoint p = [self panelPoint:event];
    int port = hs_panel_knob_at(panel, (int)p.x, (int)p.y);
    if (port < 0 || event.scrollingDeltaY == 0) return;
    float step = event.hasPreciseScrollingDeltas ? (float)event.scrollingDeltaY * .25f : (float)event.scrollingDeltaY * 2;
    float v = hs_panel_drag(panel, port, target.ctl[port], step);
    target.edit(target.ctx, HS_EDIT_BEGIN, (uint32_t)port, target.ctl[port]);
    target.edit(target.ctx, HS_EDIT_VALUE, (uint32_t)port, v);
    target.edit(target.ctx, HS_EDIT_END, (uint32_t)port, v);
    [self setNeedsDisplay:YES];
}

- (void)tick:(NSTimer *)t { (void)t; [self setNeedsDisplay:YES]; }
@end

static HSPanelView *view_of(const clap_plugin_t *p)
{
    hs_gui_target t = hs_clap_gui_target(p);
    return t.slot && *t.slot ? (__bridge HSPanelView *)*t.slot : nil;
}

static bool g_supported(const clap_plugin_t *p, const char *api, bool floating)
{
    (void)p;
    return !floating && !strcmp(api, CLAP_WINDOW_API_COCOA);
}
static bool g_preferred(const clap_plugin_t *p, const char **api, bool *floating)
{
    (void)p;
    *api = CLAP_WINDOW_API_COCOA;
    *floating = false;
    return true;
}
static bool g_create(const clap_plugin_t *p, const char *api, bool floating)
{
    if (!g_supported(p, api, floating)) return false;
    hs_gui_target t = hs_clap_gui_target(p);
    if (*t.slot) return true;
    HSPanelView *v = [[HSPanelView alloc] initWithFrame:NSMakeRect(0, 0, HS_PANEL_W * SCALE, HS_PANEL_H * SCALE)];
    v->target = t;
    v->panel = hs_panel_new(t.d, t.ctl, t.param_port, t.nparams);
    v->dragPort = -1;
    if (!v->panel) return false;
    *t.slot = (void *)CFBridgingRetain(v);
    return true;
}
static void g_destroy(const clap_plugin_t *p)
{
    hs_gui_target t = hs_clap_gui_target(p);
    if (!*t.slot) return;
    HSPanelView *v = (HSPanelView *)CFBridgingRelease(*t.slot);
    *t.slot = NULL;
    [v->timer invalidate];
    v->timer = nil;
    [v removeFromSuperview];
    hs_panel_free(v->panel);
    v->panel = NULL;
}
static bool g_set_scale(const clap_plugin_t *p, double scale) { (void)p; (void)scale; return false; }  /* Cocoa uses points */
static bool g_get_size(const clap_plugin_t *p, uint32_t *w, uint32_t *h)
{
    HSPanelView *v = view_of(p);
    *w = v ? (uint32_t)v.frame.size.width : HS_PANEL_W * SCALE;
    *h = v ? (uint32_t)v.frame.size.height : HS_PANEL_H * SCALE;
    return true;
}
static bool g_can_resize(const clap_plugin_t *p) { (void)p; return true; }
static bool g_hints(const clap_plugin_t *p, clap_gui_resize_hints_t *h)
{
    (void)p;
    *h = (clap_gui_resize_hints_t){true, true, true, HS_PANEL_W, HS_PANEL_H};
    return true;
}
static bool g_adjust(const clap_plugin_t *p, uint32_t *w, uint32_t *h)   /* whole multiples: crisp pixels */
{
    (void)p;
    uint32_t k = *w / HS_PANEL_W < *h / HS_PANEL_H ? *w / HS_PANEL_W : *h / HS_PANEL_H;
    if (k < 2) k = 2;
    if (k > 6) k = 6;
    *w = HS_PANEL_W * k;
    *h = HS_PANEL_H * k;
    return true;
}
static bool g_set_size(const clap_plugin_t *p, uint32_t w, uint32_t h)
{
    HSPanelView *v = view_of(p);
    if (!v) return false;
    [v setFrameSize:NSMakeSize(w, h)];
    [v setNeedsDisplay:YES];
    return true;
}
static bool g_set_parent(const clap_plugin_t *p, const clap_window_t *window)
{
    HSPanelView *v = view_of(p);
    if (!v || !window || !window->cocoa) return false;
    [(__bridge NSView *)window->cocoa addSubview:v];
    return true;
}
static bool g_set_transient(const clap_plugin_t *p, const clap_window_t *w) { (void)p; (void)w; return false; }
static void g_title(const clap_plugin_t *p, const char *title) { (void)p; (void)title; }
static bool g_show(const clap_plugin_t *p)
{
    HSPanelView *v = view_of(p);
    if (!v) return false;
    v.hidden = NO;
    if (!v->timer) {
        v->timer = [NSTimer timerWithTimeInterval:.04 target:v selector:@selector(tick:) userInfo:nil repeats:YES];
        [[NSRunLoop mainRunLoop] addTimer:v->timer forMode:NSRunLoopCommonModes];
    }
    return true;
}
static bool g_hide(const clap_plugin_t *p)
{
    HSPanelView *v = view_of(p);
    if (!v) return false;
    v.hidden = YES;
    [v->timer invalidate];
    v->timer = nil;
    return true;
}

const clap_plugin_gui_t hs_clap_gui = {g_supported, g_preferred, g_create, g_destroy, g_set_scale, g_get_size,
                                       g_can_resize, g_hints, g_adjust, g_set_size, g_set_parent, g_set_transient,
                                       g_title, g_show, g_hide};

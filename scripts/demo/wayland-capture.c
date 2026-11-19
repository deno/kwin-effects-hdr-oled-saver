// SPDX-License-Identifier: GPL-2.0-or-later
// Stream the sole output (including its cursor) and accept logical x/y positions on stdin.
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <wayland-client.h>
#include "fake-input.h"
#include "screencast.h"

static struct org_kde_kwin_fake_input *input;
static struct zkde_screencast_unstable_v1 *cast;
static struct wl_output *output;
static int running = 1;

static void global(void *data, struct wl_registry *registry, uint32_t id, const char *name, uint32_t version)
{
    if (!strcmp(name, "org_kde_kwin_fake_input") && version >= 4)
        input = wl_registry_bind(registry, id, &org_kde_kwin_fake_input_interface, 4);
    else if (!strcmp(name, "zkde_screencast_unstable_v1") && version >= 3)
        cast = wl_registry_bind(registry, id, &zkde_screencast_unstable_v1_interface, 3);
    else if (!strcmp(name, "wl_output") && !output)
        output = wl_registry_bind(registry, id, &wl_output_interface, 1);
}
static void removed(void *data, struct wl_registry *registry, uint32_t id) {}
static const struct wl_registry_listener globals = {global, removed};

static void created(void *data, struct zkde_screencast_stream_unstable_v1 *stream, uint32_t node)
{
    printf("%u\n", node);
    fflush(stdout);
}
static void closed(void *data, struct zkde_screencast_stream_unstable_v1 *stream) { running = 0; }
static void failed(void *data, struct zkde_screencast_stream_unstable_v1 *stream, const char *reason)
{
    fprintf(stderr, "Screencast failed: %s\n", reason);
    running = 0;
}
static const struct zkde_screencast_stream_unstable_v1_listener events = {
    .created = created, .closed = closed, .failed = failed,
};

int main(void)
{
    struct wl_display *display = wl_display_connect(NULL);
    if (!display) return 1;
    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &globals, NULL);
    if (wl_display_roundtrip(display) < 0 || !input || !cast || !output) {
        fputs("Missing KWin screencast, pointer, or output interface\n", stderr);
        return 1;
    }
    org_kde_kwin_fake_input_authenticate(input, "OLED panel demo", "Record panel hover transitions");
    struct zkde_screencast_stream_unstable_v1 *stream = zkde_screencast_unstable_v1_stream_output(
        cast, output, ZKDE_SCREENCAST_UNSTABLE_V1_POINTER_EMBEDDED);
    zkde_screencast_stream_unstable_v1_add_listener(stream, &events, NULL);
    struct pollfd fds[] = {{wl_display_get_fd(display), POLLIN, 0}, {0, POLLIN, 0}};
    while (running) {
        if (wl_display_flush(display) < 0 || poll(fds, 2, -1) < 0) return 1;
        if (fds[0].revents && wl_display_dispatch(display) < 0) return 1;
        if (fds[1].revents) {
            char line[80];
            double x, y;
            if (!fgets(line, sizeof(line), stdin)) break;
            if (sscanf(line, "%lf %lf", &x, &y) != 2) return 1;
            org_kde_kwin_fake_input_pointer_motion_absolute(input, wl_fixed_from_double(x), wl_fixed_from_double(y));
        }
    }
    wl_display_disconnect(display);
    return running ? 0 : 1;
}

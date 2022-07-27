#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct GetWindowsResult {
    Window* data;
    uint64_t size;
};
static GetWindowsResult get_windows(Display* display);
static char const* get_window_name(Display* display, Window window);
static void send_key_event_to_window(Display* display, Window window,
    KeySym keysym, int modifiers,
    bool pressed);

static int64_t get_u32(char const* message);

int main()
{
    auto display = XOpenDisplay(nullptr);

    auto windows = get_windows(display); // FIXME: Free everything.
    for (int32_t window_id = 0; window_id < windows.size; window_id++) {
        auto name = get_window_name(display, windows.data[window_id]);
        printf("%d: %s\n", window_id, name);
    }
    auto maybe_window_id = get_u32("select window: ");
    if (maybe_window_id < 0 || maybe_window_id >= windows.size)
        return fprintf(stderr, "window number is out of range\n"), -1;
    auto window_id = (uint32_t)maybe_window_id;
    auto window = windows.data[window_id];

    char keystrokes[1024];
    auto keystrokes_size = sizeof(keystrokes);
    printf("keystrokes: ");
    fflush(stdout);
    fgets(keystrokes, keystrokes_size, stdin);
    for (uint32_t i = 0; i < keystrokes_size; i++) {
        if (keystrokes[i] == '\n') {
            keystrokes[i] = '\0';
            keystrokes_size = i;
            break;
        }
    }

    for (uint32_t i = 0; i < keystrokes_size; i++) {
        auto character = keystrokes[i];
        send_key_event_to_window(display, window, character, 0, false);
        send_key_event_to_window(display, window, character, 0, true);
        send_key_event_to_window(display, window, character, 0, false);
    }
    XFlush(display);
}

struct GetPropertyResult {
    char const* data;
    uint64_t size;
};
static GetPropertyResult get_property(Display* display, Window window,
    Atom property_type, char const* prop_name)
{
    auto property_name = XInternAtom(display, prop_name, False);

    uint64_t return_number_of_items;
    Atom return_type = 0;
    int32_t format_size = 0;
    uint64_t bytes_after = 0;
    uint8_t* data;
    if (XGetWindowProperty(display, window, property_name, 0, 4096 / 4, False,
            property_type, &return_type, &format_size,
            &return_number_of_items, &bytes_after, &data)
        != Success) {
        return { nullptr, 0 };
    }

    if (return_type != property_type) {
        XFree(data);
        return { nullptr, 0 };
    }

    auto size = (format_size / 4) * return_number_of_items;
    auto ret = (char*)malloc(size + 1);
    memcpy(ret, data, size);
    ret[size] = '\0';
    XFree(data);

    return { (char const*)ret, size };
}

static char const* get_window_name(Display* display, Window window)
{
    auto utf8_atom = XInternAtom(display, "UTF8_STRING", false);

    auto name = get_property(display, window, utf8_atom, "_NET_WM_NAME");
    if (!name.data)
        name = get_property(display, window, XA_STRING, "WM_NAME");

    return (char const*)name.data;
}

static XKeyEvent create_key_event(Display* display, Window root,
    Window window, bool press,
    KeySym keysym, int modifiers)
{
    XKeyEvent event;
    event.type = press ? KeyPress : KeyRelease;
    event.display = display;
    event.window = window;
    event.root = root;
    event.subwindow = None;
    event.time = CurrentTime;
    event.x = 0;
    event.y = 0;
    event.x_root = 0;
    event.y_root = 0;
    event.same_screen = true;
    event.keycode = XKeysymToKeycode(display, keysym);
    event.state = modifiers;

    return event;
}

static void send_key_event(XKeyEvent event)
{
    XSendEvent(event.display, event.window, false, 0, (XEvent*)&event);
}

static void send_key_event_to_window(Display* display, Window window,
    KeySym keysym, int modifiers, bool pressed)
{
    auto screen = XDefaultScreen(display);
    auto root = XRootWindow(display, screen);
    auto event = create_key_event(display, root, window, pressed, keysym, modifiers);
    send_key_event(event);
}

static GetWindowsResult get_windows(Display* display)
{
    auto root = DefaultRootWindow(display);
    auto windows = get_property(display, root, XA_WINDOW, "_NET_CLIENT_LIST");
    if (!windows.data)
        windows = get_property(display, root, XA_CARDINAL, "_WIN_CLIENT_LIST");

    return { (Window*)windows.data, windows.size / sizeof(Window) };
}

int64_t get_u32(char const* message)
{
    printf("%s", message);
    fflush(stdout);

    char buf[12];
    fgets(buf, sizeof(buf), stdin);
    if (buf[0] == '\n')
        return -1;

    char* end_pointer = nullptr;
    errno = 0;
    auto value = strtoul(buf, &end_pointer, 10);
    if (errno)
        return -1;
    if (end_pointer == buf)
        return -1;

    return value;
}

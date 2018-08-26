#include <stdio.h>
#include <windows.h>
#include <interception.h>

// How long to ignore trackpad events for after keyboard events.
#define IGNORE_MOUSE_MS 300
#define MAX_HARDWARE_ID 500

#define APPLE_TRACKPAD_ID "HID\\VID_05AC&PID_0262&REV_0222&MI_01&Col01"
#define APPLE_NATIVE_KEYBOARD "HID\\VID_05AC&PID_0262&REV_0222&MI_00&Col01"

typedef struct {
	DWORD ignoreTrackpadTill;
	InterceptionDevice keyboard;
	InterceptionDevice trackpad;
} state;

// Hacky global for the custom predicate.
InterceptionDevice g_want_device;


int find_device(InterceptionContext ctx, const char* name);
void check_keyboard(state* s, DWORD curTime, InterceptionDevice device, InterceptionStroke *stroke);
int block_mouse_event(state* s, DWORD curTime, InterceptionDevice device, InterceptionStroke *stroke);
InterceptionContext check_devices(state *s, InterceptionDevice device);

int interception_is_our_device(InterceptionDevice device) {
	return device == g_want_device;
}

int main(int argc, char** argv) {
	// Increase priority since we are responding to events.
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	InterceptionContext ctx = interception_create_context();
	state s;
	s.ignoreTrackpadTill = 0;
	s.trackpad = find_device(ctx, APPLE_TRACKPAD_ID);
	s.keyboard = find_device(ctx, APPLE_NATIVE_KEYBOARD);

	if (s.trackpad == 0) {
		printf("Failed to find trackpad with specified ID: %s", APPLE_TRACKPAD_ID);
		return 1;
	}
	if (s.keyboard == 0) {
		printf("Failed to find keyboard with specified ID: %s", APPLE_TRACKPAD_ID);
		return 1;
	}

	// Add filters for the specific devices we found.
	g_want_device = s.keyboard;
	interception_set_filter(ctx, interception_is_our_device, INTERCEPTION_FILTER_KEY_ALL);

	g_want_device = s.trackpad;
	interception_set_filter(ctx, interception_is_our_device, INTERCEPTION_FILTER_MOUSE_ALL & ~INTERCEPTION_FILTER_MOUSE_MOVE);


	InterceptionDevice device;
	InterceptionStroke stroke;
	while (interception_receive(ctx, device = interception_wait(ctx), &stroke, 1) > 0) {
		DWORD curTime = GetTickCount();
		check_keyboard(&s, curTime, device, &stroke);
		if (block_mouse_event(&s, curTime, device, &stroke)) {
			continue;
		}

		interception_send(ctx, device, &stroke, 1);
	}

	interception_destroy_context(ctx);
	return 0;
}

int find_device(InterceptionContext ctx, const char* name) {
	wchar_t passed_id[MAX_HARDWARE_ID];
	int length = swprintf_s(passed_id, MAX_HARDWARE_ID-1, L"%hs", name);
	passed_id[length] = '\0';

	wchar_t hardware_id[MAX_HARDWARE_ID];
	for (InterceptionDevice device = 1; device <= INTERCEPTION_MAX_DEVICE; device++) {
		size_t length = interception_get_hardware_id(ctx, device, hardware_id, sizeof(hardware_id));
		if (length == 0) {
			continue;
		}
		if (length >= sizeof(hardware_id)) {
			length = sizeof(hardware_id) - 1;
		}
		hardware_id[length] = '\0';

		printf("Comparing device %d with name %S to %S\n", device, hardware_id, passed_id);
		if (wcsncmp(passed_id, hardware_id, MAX_HARDWARE_ID) == 0) {
			return device;
		}
	}

	return 0;
}


void check_keyboard(state* s, DWORD curTime, InterceptionDevice device, InterceptionStroke *stroke) {
	if (device != s->keyboard) {
		return;
	}

	// Ignore all mouse events for a certain amount of time.
	s->ignoreTrackpadTill = curTime + IGNORE_MOUSE_MS;
	InterceptionKeyStroke *keyStroke = (InterceptionKeyStroke *)stroke;
	printf("Got keyboard event: %d\n", keyStroke->code);
}

// block_mouse_event returns whether to block the mouse event.
int block_mouse_event(state* s, DWORD curTime, InterceptionDevice device, InterceptionStroke *stroke) {
	if (device != s->trackpad) {
		return 0;
	}

	InterceptionMouseStroke *mouseStroke = (InterceptionMouseStroke *)stroke;
	printf("Got mouse event: %d\n", mouseStroke->state);

	if (curTime > s->ignoreTrackpadTill) {
		return 0;
	}

	printf("!Ignoring mouse event!\n");
	return 1;
}
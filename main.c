#include <stdio.h>
#include <windows.h>
#include <interception.h>

// The maximum string length of a hardware ID.
#define MAX_HARDWARE_ID 500

#define DEBUGF(...) if (0) { printf(##__VA_ARGS__); }

typedef struct {
	int ignorePeriod;
	DWORD ignoreTrackpadTill;
	InterceptionDevice keyboard;
	InterceptionDevice trackpad;
	InterceptionKeyStroke lastKey;
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

	if (argc < 3) {
		printf("Expect ./binary <trackpad_hardware_id> <mouse_hardware_id> [delay]\n");
		return EXIT_FAILURE;
	}

	// Parse out device IDs from arguments.
	char* trackpad_id = argv[1];
	char* keyboard_id = argv[2];

	int ignorePeriod = 300;
	if (argc > 3) {
		ignorePeriod = atoi(argv[3]);
	}

	InterceptionContext ctx = interception_create_context();
	state s;
	s.ignorePeriod = ignorePeriod;
	s.ignoreTrackpadTill = 0;
	s.trackpad = find_device(ctx, trackpad_id);
	s.keyboard = find_device(ctx, keyboard_id);

	if (s.trackpad == 0) {
		printf("Failed to find trackpad with specified ID: %s", trackpad_id);
		return EXIT_FAILURE;
	}
	if (s.keyboard == 0) {
		printf("Failed to find keyboard with specified ID: %s", keyboard_id);
		return EXIT_FAILURE;
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

	InterceptionKeyStroke *keyStroke = (InterceptionKeyStroke *)stroke;
	DEBUGF("Got keyboard code: %d state: %d information: %ud\n", keyStroke->code, keyStroke->state, keyStroke->information);
	if (memcmp(keyStroke, &s->lastKey, sizeof(InterceptionKeyStroke)) == 0) {
		DEBUGF("  ignoring duplicate event, likely a repeat\n");
	} else {
		// Ignore all mouse events for a certain amount of time.
		s->ignoreTrackpadTill = curTime + s->ignorePeriod;
		DEBUGF("  ignoring mouse for %d ms\n", s->ignorePeriod);
	}

	s->lastKey = *keyStroke;
}

// block_mouse_event returns whether to block the mouse event.
int block_mouse_event(state* s, DWORD curTime, InterceptionDevice device, InterceptionStroke *stroke) {
	if (device != s->trackpad) {
		return 0;
	}

	InterceptionMouseStroke *mouseStroke = (InterceptionMouseStroke *)stroke;
	DEBUGF("Got mouse event: %d\n", mouseStroke->state);

	if (curTime > s->ignoreTrackpadTill) {
		return 0;
	}

	printf("!Ignoring mouse event %d!\n", mouseStroke->state);
	return 1;
}
#include <stdio.h>
#include <windows.h>
#include <interception.h>

// How long to ignore trackpad events for after keyboard events.
#define IGNORE_MOUSE_MS 300
#define MAX_HARDWARE_ID 500


typedef struct {
	DWORD ignoreTrackpadTill;
	InterceptionDevice keyboard;
	InterceptionDevice trackpad;
} state;

void check_keyboard(state* s, DWORD curTime, InterceptionDevice device, InterceptionStroke *stroke);
int block_mouse_event(state* s, DWORD curTime, InterceptionDevice device, InterceptionStroke *stroke);
InterceptionContext check_devices(state *s, InterceptionDevice device);


int main(int argc, char** argv) {
	// Increase priority since we are responding to events.
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	// Add filters for all keys and all mouse actions except move
	InterceptionContext ctx = interception_create_context();
	interception_set_filter(ctx, interception_is_mouse, INTERCEPTION_FILTER_MOUSE_ALL & ~INTERCEPTION_FILTER_MOUSE_MOVE);
	interception_set_filter(ctx, interception_is_keyboard, INTERCEPTION_FILTER_KEY_ALL);

	state s;
	s.keyboard = 0;
	s.trackpad = 0;
	s.ignoreTrackpadTill = 0;

	DWORD ignoreTrackpadTill = 0;
	InterceptionDevice device;
	InterceptionStroke stroke;
	while (interception_receive(ctx, device = interception_wait(ctx), &stroke, 1) > 0) {
		printf("device is %d\n", device);

		InterceptionContext new_ctx = check_devices(&s, device);
		if (new_ctx != NULL) {
			printf("replaced device discovery ctx with simpler context!\n");
			interception_destroy_context(ctx);
			ctx = new_ctx;
		}

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

int check_keyboard_device(state *s, InterceptionDevice device) {
	if (s->keyboard) {
		return 1;
	}

	// TODO: Check if the hardware ID matches
	if (device == 1) {
		printf("found keyboard\n");
		s->keyboard = 1;
		return 1;
	}

	return 0;
}

int check_trackpad_device(state *s, InterceptionDevice device) {
	if (s->trackpad) {
		return 1;
	}

	// TODO: Check if the hardware ID matches
	if (device == 11) {
		printf("found mouse\n");
		s->trackpad = 11;
		return 1;
	}

	return 0;
}


InterceptionDevice g_want_device;
int interception_is_our_device(InterceptionDevice device) {
	return device == g_want_device;
}


InterceptionContext check_devices(state *s, InterceptionDevice device) {
	if (s->keyboard && s->trackpad) {
		return NULL;
	}

	if (check_trackpad_device(s, device) + check_keyboard_device(s, device) != 2) {
		return NULL;
	}

	// We have both devices set (and they weren't set at the beginning). Which means
	// we can simplify our context.
	InterceptionContext ctx = interception_create_context();

	g_want_device = s->keyboard;
	interception_set_filter(ctx, interception_is_our_device, INTERCEPTION_FILTER_KEY_ALL);

	g_want_device = s->trackpad;
	interception_set_filter(ctx, interception_is_our_device, INTERCEPTION_FILTER_MOUSE_ALL & ~INTERCEPTION_FILTER_MOUSE_MOVE);

	return ctx;
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
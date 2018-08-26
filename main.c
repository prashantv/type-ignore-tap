#include <stdio.h>
#include <windows.h>
#include <interception.h>

// How long to ignore trackpad events for after keyboard events.
#define IGNORE_MOUSE_MS 300
#define MAX_HARDWARE_ID 500

int main(int argc, char** argv) {
	// Increase priority since we are responding to events.
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);


	// Add filters for all keys and all mouse actions except move
	InterceptionContext ctx = interception_create_context();
	interception_set_filter(ctx, interception_is_mouse, INTERCEPTION_FILTER_MOUSE_ALL & ~INTERCEPTION_FILTER_MOUSE_MOVE);
	interception_set_filter(ctx, interception_is_keyboard, INTERCEPTION_FILTER_KEY_ALL);

	DWORD ignoreTrackpadTill = 0;
	InterceptionDevice device;
	InterceptionStroke stroke;
	while (interception_receive(ctx, device = interception_wait(ctx), &stroke, 1) > 0) {
		DWORD curTime = GetTickCount();
		if (interception_is_keyboard(device)) {
			// Ignore all mouse events for a certain amount of time.
			ignoreTrackpadTill = curTime + IGNORE_MOUSE_MS;
			InterceptionKeyStroke *keyStroke = (InterceptionKeyStroke *)&stroke;
			printf("Got keyboard event: %d\n", keyStroke->code);
		}

		// Check whether we're within the ignore time.
		if (interception_is_mouse(device)) {
			InterceptionMouseStroke *mouseStroke = (InterceptionMouseStroke *)&stroke;
			printf("Got mouse event: %d\n", mouseStroke->state);

			if (curTime < ignoreTrackpadTill) {
				printf("!Ignoring mouse event!\n");
				continue;
			}
		}
		
		interception_send(ctx, device, &stroke, 1);
	}

	interception_destroy_context(ctx);
	return 0;
}



int interception_is_apple(InterceptionDevice device) {

}


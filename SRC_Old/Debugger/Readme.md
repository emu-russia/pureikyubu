# Debugger

This component deals with the usual debugging tasks that all developers want to do:

- Collecting debug messages
- Tracing
- Code profiling
- Performance counters

This component no longer has anything to do with the debug console. The debug console code has been moved to the category of user interfaces and moved to the UI\\Legacy folder.

## Debug report notes

Previously, debug messages would pass into the internals of the debug console (cmd_print).

Debug messages are now a self-contained entity and are stored in the debug message queue.

Debug UI (or whoever claims it), when active, should periodically ask - "Is there something?". If there is, the current messages from the queue are transferred to the debug UI and the queue is cleared.

Also, in order not to occupy all the user's memory, debug messages are cleared by themselves after reaching a certain limit of messages.

#ifndef GUARD_724b255a_2d89_4e5b_8537_e04103a75485
#define GUARD_724b255a_2d89_4e5b_8537_e04103a75485
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>

// Implementation details: the cond is fired by the consumer when it's done,
// and if the adder is waiting, and the done flag isn't set, it will unlock the mutex and be done.
// Regardless, the consumer always locks the mutex to change the state.
// Usually, the consumer is responsible for cleaning, but if the consumer is done early, the joiner cleans up
typedef struct {
    pthread_cond_t cond;    // fired by consumer when done
    pthread_mutex_t mutex;  // locked by adder when joining, and locked by consumer to set the done flag
    char state;             // '\0' at start, 'W' when joiner was waiting, 'D' when done early and 'T' detached
} EventQueue_JoinHandle;

struct _eventQueue_Event {
    EventQueue_JoinHandle *join;  // may be NULL if joining isn't required
    void *data;
    struct _eventQueue_Event *next;
};

typedef struct {
    pthread_cond_t new_event_ready;  // fired when there's an event to be pushed and someone's locking for a new event
    pthread_mutex_t event_lock;      // locked to set an event or pop an event

    _eventQueue_Event *head;
    _eventQueue_Event *tail;
} EventQueue;

// Create a new EventQueue
EventQueue *EventQueue_new();
// Release an EventQueue, must be done when nobody's using it
void EventQueue_free(EventQueue *eq);

// Add an event to an EventQueue, which can't be joined
void EventQueue_add(EventQueue *eq, void *data);
// If any thread is blocking for an event,
// add to the EventQueue and return true, otherwise return false.
// Works on whether the event_lock is locked.
bool EventQueue_add_if_any_waiting(EventQueue *eq);

// Add an event to an EventQueue, which can be joined
EventQueue_JoinHandle *EventQueue_add_joinable(EventQueue *eq, void *data);
// Wait for the event to be handled
void EventQueue_join(EventQueue_JoinHandle *handle);
// Nevermind, I don't care when the event is done
void EventQueue_detach(EventQueue_JoinHandle *handle);

typedef struct {
    EventQueue *eq;
    EventQueue_JoinHandle *join;  // may be NULL if nobody's expecting to join
} EventQueue_Consumer;

EventQueue_Consumer *EventQueue_new_consumer(EventQueue *eq);
// Take the first event from an EventQueue and get the data, and tell joining adders that you're done.
void *EventQueue_consume(EventQueue_Consumer *consumer);
// Destroy the consumer, and tell any joining adders than you're done.
void EventQueue_destroy_consumer(EventQueue_Consumer *consumer);

#endif

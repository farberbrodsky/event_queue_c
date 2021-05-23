#include "event_queue.h"

EventQueue *EventQueue_new() {
    EventQueue *result = calloc(1, sizeof(EventQueue));
    pthread_cond_init(&result->new_event_ready, NULL);
    pthread_mutex_init(&result->event_lock, NULL);
    return result;
}

void EventQueue_free(EventQueue *eq) {
    pthread_cond_destroy(&eq->new_event_ready);
    pthread_mutex_destroy(&eq->event_lock);
}

static void EventQueue_add_mayjoin(EventQueue *eq, struct _eventQueue_Event *event) {
    pthread_mutex_lock(&eq->event_lock);    // lock the event queue
    if (eq->tail == NULL) {
        eq->head = event;
        eq->tail = event;
    } else {
        eq->tail->next = event;
        eq->tail = event;
    }
    pthread_mutex_unlock(&eq->event_lock);  // unlock the event queue
}

void EventQueue_add(EventQueue *eq, void *data) {
    struct _eventQueue_Event *event = calloc(1, sizeof(struct _eventQueue_Event));
    event->data = data;
    EventQueue_add_mayjoin(eq, event);
}

// TODO i must differentiate between locking for adding or removing an event
bool EventQueue_add_if_any_waiting(EventQueue *eq);

EventQueue_JoinHandle *EventQueue_add_joinable(EventQueue *eq, void *data) {
    struct _eventQueue_Event *event = calloc(1, sizeof(struct _eventQueue_Event));
    EventQueue_JoinHandle *join = malloc(sizeof(EventQueue_JoinHandle));
    pthread_cond_init(&join->cond, NULL);
    pthread_mutex_init(&join->mutex, NULL);
    join->state = '\0';

    event->join = join;
    event->data = data;
    EventQueue_add_mayjoin(eq, event);
}

static void EventQueue_JoinHandle_cleanup(EventQueue_JoinHandle *handle) {
    pthread_cond_destroy(&handle->cond);
    pthread_mutex_destroy(&handle->mutex);
    free(handle);
}

void EventQueue_join(EventQueue_JoinHandle *handle) {
    pthread_mutex_lock(&handle->mutex);
    if (handle->state == 'D') {
        // Consumer was done early, I need to clean this
        pthread_mutex_unlock(&handle->mutex);
        EventQueue_JoinHandle_cleanup(handle);
    } else {
        // I'm waiting...
        handle->state = 'W';
        pthread_cond_wait(&handle->cond, &handle->mutex);
    }
}

static void EventQueue_event_done(EventQueue_JoinHandle *handle) {
    pthread_cond_signal(&handle->cond);  // fire this if the adder is waiting
    pthread_mutex_lock(&handle->mutex);
    if (handle->state == 'T') {
        // detached, I have to clean this
        pthread_mutex_unlock(&handle->mutex);
        EventQueue_JoinHandle_cleanup(handle);
    } else if (handle->state == 'W') {
        // joiner has been waiting but isn't anymore due to signal, I will clean this
        pthread_mutex_unlock(&handle->mutex);
        EventQueue_JoinHandle_cleanup(handle);
    } else {// handle->state == '\0'
        // tell the adder I'm done early
        handle->state = 'D';
        pthread_mutex_unlock(&handle->mutex);
    }
}

void EventQueue_detach(EventQueue_JoinHandle *handle) {
    pthread_mutex_lock(&handle->mutex);
    handle->state = 'T';
    pthread_mutex_unlock(&handle->mutex);
}

EventQueue_Consumer *EventQueue_new_consumer(EventQueue *eq) {
    EventQueue_Consumer *consumer = malloc(sizeof(EventQueue_Consumer));
    consumer->eq = eq;
    consumer->join = NULL;
}

void *EventQueue_consume(EventQueue_Consumer *consumer);  // LAST TODO

void EventQueue_destroy_consumer(EventQueue_Consumer *consumer) {
    if (consumer->join != NULL) {
        EventQueue_event_done(consumer->join);
    }
    free(consumer);
}

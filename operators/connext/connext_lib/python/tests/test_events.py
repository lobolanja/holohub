from unittest import TestCase

from connext_lib.payload_io.interprocess_events import Event, EventSubscriber, _EventBus

class TestEventSystem(TestCase):
    def setUp(self):
        # Reset shared memory before each test
        _EventBus._init_memory()
        _EventBus._write_map({})

    def test_event_notify_and_subscriber_poll(self):
        event_name = "test_event"
        subscriber_id = "sub1"
        event = Event(event_name)
        subscriber = EventSubscriber(event_name, subscriber_id)

        # Initially, poll should be False
        self.assertFalse(subscriber.poll())

        # Notify event, poll should be True
        event.notify()
        self.assertTrue(subscriber.poll())

        # After consuming, poll should be False again
        self.assertFalse(subscriber.poll())

    def test_multiple_subscribers(self):
        event_name = "multi_event"
        sub_ids = ["subA", "subB"]
        event = Event(event_name)
        subs = [EventSubscriber(event_name, sid) for sid in sub_ids]

        # All polls should be False initially
        for sub in subs:
            self.assertFalse(sub.poll())

        # Notify event
        event.notify()
        for sub in subs:
            self.assertTrue(sub.poll())
            # After consuming, poll should be False
            self.assertFalse(sub.poll())

    def test_subscriber_wait(self):
        event_name = "wait_event"
        subscriber_id = "sub_wait"
        event = Event(event_name)
        subscriber = EventSubscriber(event_name, subscriber_id)

        result = subscriber.wait(1.0)

        self.assertFalse(result)
        event.notify()
        result = subscriber.wait(1.0)
        self.assertTrue(result)
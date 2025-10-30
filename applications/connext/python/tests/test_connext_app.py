import uuid

import pytest

from connext_demo import ConnextAnoLoopbackApp, DemoAppConfig


@pytest.mark.parametrize("transport", ["ano", "dds"])
def test_connext_demo_loopback(transport):
    base_payload = f"demo_payload_{transport}"
    shm_suffix = uuid.uuid4().hex[:8]

    config = DemoAppConfig(
        payload=base_payload,
        message_count=2,
        shm_name_tx=f"connext_demo_tx_{shm_suffix}",
        shm_name_rx=f"connext_demo_rx_{shm_suffix}",
        shm_event_name=f"connext_demo_evt_{shm_suffix}",
        use_dds=transport == "dds",
        dds_domain_id=3,
        dds_topic_name=f"DemoTopic{shm_suffix}",
    )

    app = ConnextAnoLoopbackApp(config)
    app.run()

    received = app.received_payloads
    assert len(received) > 0

    if transport == "ano":
        assert any(base_payload.encode("utf-8") in payload for payload in received)
    else:
        found = False
        for payload in received:
            if received != []:
                if base_payload in payload[0].data:
                    found = True
        assert found
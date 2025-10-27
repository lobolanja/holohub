import argparse
import contextlib
import logging

from connext_demo import ConnextAnoLoopbackApp, DemoAppConfig


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run the Connext ANO loopback demo application.")
    parser.add_argument("--transport", choices=("ano", "dds"), default="ano", help="Transport mechanism to use.")
    parser.add_argument("--payload", default="hello_holoscan", help="Base payload string to broadcast.")
    parser.add_argument("--count", type=int, default=3, help="Number of payloads to emit.")
    parser.add_argument("--domain-id", type=int, default=2, help="DDS domain ID when transport=dds.")
    parser.add_argument("--rx-shm-name", default="connext_demo_rx", help="Shared memory name for the receiver when using ANO.")
    parser.add_argument("--tx-shm-name", default="connext_demo_tx", help="Shared memory name for the transmitter when using ANO.")
    parser.add_argument("--event-name", default="connext_demo_event", help="Event name for notifying available buffers.")
    return parser.parse_args()


def main():
    logging.basicConfig(level=logging.INFO)
    args = parse_args()

    config = DemoAppConfig(
        payload=args.payload,
        message_count=args.count,
        shm_name_tx=args.tx_shm_name,
        shm_name_rx=args.rx_shm_name,
        shm_event_name=args.event_name,
        dds_domain_id=args.domain_id,
        use_dds=args.transport == "dds",
    )

    app = ConnextAnoLoopbackApp(config)
    try:
        app.run()
    except KeyboardInterrupt:
        logging.info("Interrupt received, stopping the Connext demo.")
        stop = getattr(app, "stop", None)
        if callable(stop):
            # Some Holoscan applications expose an explicit stop method.
            with contextlib.suppress(Exception):
                stop()
        logging.info("Connext demo stopped.")


if __name__ == "__main__":
    main()

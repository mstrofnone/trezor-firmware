import pytest

from trezorlib.prodtest.prodtest_client import ProdtestClient
from trezorlib.prodtest.prodtest_emulator import get_prodtest_emulator
from trezorlib.prodtest.prodtest_transport import VcpUdpTransport


@pytest.fixture(scope="session")
def prodtest_client():
    """Start a prodtest emulator and yield a connected client."""
    emu = get_prodtest_emulator()
    with emu:
        client = ProdtestClient(transport=VcpUdpTransport())
        yield client
        client.close()

# This file is part of the Trezor project.
#
# This library is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License version 3
# as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the License along with this library.
# If not, see <https://www.gnu.org/licenses/lgpl-3.0.html>.

import time

import pytest

from trezorlib import messages
from trezorlib.debuglink import DebugSession as Session


def ping_without_ack(session: Session):
    br = session.call_raw(messages.Ping(message="delayed", button_protection=True))
    assert isinstance(br, messages.ButtonRequest)
    assert br.code == messages.ButtonRequestType.ProtectCall
    # confirm layout before ButtonAck is sent
    session.debug.press_yes()
    # "waiting" screen should be shown after 2 seconds on Core models
    # (following https://github.com/trezor/trezor-firmware/issues/5884)
    time.sleep(2.5)


def test_delayed_ack(session: Session):
    ping_without_ack(session)

    res = session.call_raw(messages.ButtonAck())
    res = messages.Success.ensure_isinstance(res)
    assert res.message == "delayed"


@pytest.mark.models("core")
def test_abort(session: Session):
    ping_without_ack(session)

    session.debug.press_yes()
    assert session.read() == messages.Failure(
        code=messages.FailureType.ActionCancelled, message="Cancelled"
    )
    assert session.client.ping("again") == "again"

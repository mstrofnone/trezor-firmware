# This file is part of the Trezor project.
#
# Copyright (C) SatoshiLabs and contributors
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

from __future__ import annotations

import logging
from abc import ABCMeta, abstractmethod

LOG = logging.getLogger(__name__)


class ProdtestTransport(metaclass=ABCMeta):

    @abstractmethod
    def readline(self, timeout: float | None = None) -> str:
        raise NotImplementedError

    @abstractmethod
    def writeline(self, line: str) -> None:
        raise NotImplementedError

    @abstractmethod
    def close(self) -> None:
        raise NotImplementedError


class ProdtestCommand:

    def __init__(self, name: str, *args: str) -> None:
        self.name = name
        self.args = list(args)
        self.cmd_line = " ".join([self.name] + self.args)

    def get(self) -> str:
        return self.cmd_line


class ProdtestResponse:
    def __init__(
        self,
        is_ok: bool,
        args: str,
        progress: list[str] | None = None,
        traces: list[str] | None = None,
    ) -> None:
        self.is_ok = is_ok
        self.args = args
        self.progress = progress
        self.traces = traces


class ProdtestClient:
    DEFAULT_TIMEOUT: float = 30

    def __init__(self, transport: ProdtestTransport) -> None:
        self.transport = transport

    def command(
        self, cmd: ProdtestCommand, timeout: float = DEFAULT_TIMEOUT
    ) -> ProdtestResponse:

        self.transport.writeline(cmd.get())

        progress_lines: list[str] = []
        trace_lines: list[str] = []

        while True:
            line = self.transport.readline(timeout)
            if line.startswith("OK"):
                ok_args = line[2:].strip()
                return ProdtestResponse(
                    is_ok=True,
                    args=ok_args,
                    progress=progress_lines,
                    traces=trace_lines,
                )
            elif line.startswith("ERROR"):
                err_args = line[5:].strip()
                return ProdtestResponse(
                    is_ok=False,
                    args=err_args,
                    progress=progress_lines,
                    traces=trace_lines,
                )
            elif line.startswith("PROGRESS"):
                progress_lines.append(line[8:].strip())
            elif line.startswith("#"):
                trace_lines.append(line[1:].strip())
            else:
                LOG.warning("Unexpected line from prodtest: %s", line)

    def close(self) -> None:
        self.transport.close()

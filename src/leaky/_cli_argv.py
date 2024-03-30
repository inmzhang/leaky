import sys

import leaky


def cli_argv():
    leaky.cli(command_line_args=sys.argv[1:])
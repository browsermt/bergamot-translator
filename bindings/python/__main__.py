import argparse
import sys
from argparse import ArgumentParser

from .cmds import CMDS, make_parser


def main() -> None:
    parser = make_parser()
    args = parser.parse_args()

    if args.action in CMDS:
        CMDS[args.action].execute(args)
    else:
        parser.print_help(sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()

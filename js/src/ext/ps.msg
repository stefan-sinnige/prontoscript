/*
 * This file is part of the ProntoScript replication distribution
 * (https://github.com/stefan-sinnige/prontoscript)
 *
 * Copyright (C) 2025, Stefan Sinnige <stefan@kalion.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * Error messages for the ProntoScript extension.
 *
 * The format for each JS error message is:
 *
 * MSG_DEF(<SYMBOLIC_NAME>, <ERROR_NUMBER>, <ARGUMENT_COUNT>, <EXCEPTION_NAME>,
 *         <FORMAT_STRING>)
 *
 * where ;
 * <SYMBOLIC_NAME> is a legal C identifer that will be used in the
 * JS engine source.
 *
 * <ERROR_NUMBER> is an unique integral value identifying this error.
 *
 * <ARGUMENT_COUNT> is an integer literal specifying the total number of
 * replaceable arguments in the following format string.
 *
 * <EXCEPTION_NAME> is an exception index from the enum in jsexn.c;
 * JSEXN_NONE for none.  The given exception index will be raised by the
 * engine when the corresponding error occurs.
 *
 * <FORMAT_STRING> is a string literal, optionally containing sequences
 * {X} where X  is an integer representing the argument number that will
 * be replaced with a string value when the error is reported.
 */

MSG_DEF(PSMSG_NOT_ENOUGH_ARGUMENTS,      209, 0, JSEXN_ERR, "not enough arguments specified")
MSG_DEF(PSMSG_ARGUMENT_NOT_A_FUNCTION,   210, 0, JSEXN_ERR, "argument is not a function")
MSG_DEF(PSMSG_INVALID_EVENT_TYPE,        211, 0, JSEXN_TYPEERR, "invalid event type")
MSG_DEF(PSMSG_NO_ARGUMENT,               212, 0, JSEXN_ERR, "no argument specified")
MSG_DEF(PSMSG_ARGUMENT_NOT_INT,          213, 0, JSEXN_TYPEERR, "argument is not an integer")
MSG_DEF(PSMSG_ARGUMENT_NOT_POSITIVE_INT, 214, 0, JSEXN_TYPEERR, "argument is not a positive integer number")
MSG_DEF(PSMSG_ARGUMENT_NOT_STRING,       215, 0, JSEXN_TYPEERR, "argument is not a string")
MSG_DEF(PSMSG_ARGUMENT_OUT_OF_RANGE,     216, 0, JSEXN_TYPEERR, "argument out of range")
MSG_DEF(PSMSG_INVALID_NAME,              217, 0, JSEXN_ERR, "invalid name")
MSG_DEF(PSMSG_MAXIMUM_SOCKETS,           218, 0, JSEXN_LIMIT, "maximum active socket count reached")
MSG_DEF(PSMSG_FAILED_TO_CONNECT,         219, 0, JSEXN_ERR, "failed to connect")
MSG_DEF(PSMSG_FAILED,                    220, 1, JSEXN_ERR, "failed: {0}")
MSG_DEF(PSMSG_SOCKET_ERROR,              221, 0, JSEXN_ERR, "socket error")
MSG_DEF(PSMSG_SOCKET_BLOCKING,           222, 0, JSEXN_ERR, "socket is blocking")
MSG_DEF(PSMSG_SOCKET_NOT_READY,          223, 0, JSEXN_ERR, "socket not ready")
MSG_DEF(PSMSG_ADDRESS_IN_USE,            224, 0, JSEXN_ERR, "address already in use")
MSG_DEF(PSMSG_INSUFFICIENT_MEMORY,       225, 0, JSEXN_ERR, "insufficient internal memory available")
MSG_DEF(PSMSG_MAXIMUM_BLOCKING_READ,     226, 0, JSEXN_ERR, "maximum blocking read length exceeded")
MSG_DEF(PSMSG_NOT_ALL_TEST_CASES_RUN,    227, 0, JSEXN_ERR, "not all test cases have run")
MSG_DEF(PSMSG_FAILING_TEST_SUITE,        228, 0, JSEXN_ERR, "failing test suite")


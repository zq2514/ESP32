/**
 * @file commands.h
 * @brief Serial command-line interface.
 */

#ifndef COMMANDS_H
#define COMMANDS_H

/** Print startup banner (pin map, project name). */
void commands_print_banner();

/** Non-blocking: process one serial line if available. */
void commands_process();

#endif

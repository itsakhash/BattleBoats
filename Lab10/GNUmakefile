##
# @file		GNUmakefile
#
# This Makefile is used for building the sample test harnesses for the Agent, 
# Field, Message, and Negotiation modules used for Lab10.
#
# @usage	`$ make <MODULE>_test`
#
# @author  HARE Lab
# @author  jLab
# @version 1.0.0
#
# @date		30 Nov 2024
#
# @todo		Emulate OLED functions.
##

# Compiler and flags.
CC := gcc 
CFLAGS := -Wall -Wextra -g

# Include paths.
COMMON_DIR := ../Common
INCLUDES := -I$(COMMON_DIR) -Iinclude

# Source files.
AGENT_SRCS := src/AgentTest.c src/Agent.c src/Field.c src/Negotiation.c $(COMMON_DIR)/BOARD.c
FIELD_SRCS := src/FieldTest.c src/Field.c $(COMMON_DIR)/BOARD.c
MESSAGE_SRCS := src/MessageTest.c src/Message.c
NEGOTIATION_SRCS := src/NegotiationTest.c src/Negotiation.c

# Uncomment the default target of your dreams.
SRCS := $(AGENT_SRCS) $(FIELD_SRCS) $(MESSAGE_SRCS) $(NEGOTIATION_SRCS)

# Object files.
AGENT_OBJS := $(AGENT_SRCS:.c=.o)
FIELD_OBJS := $(FIELD_SRCS:.c=.o)
MESSAGE_OBJS := $(MESSAGE_SRCS:.c=.o)
NEGOTIATION_OBJS := $(NEGOTIATION_SRCS:.c=.o)
OBJS := $(SRCS:.c=.o)

# Targets.
# Adjust this to your preference.
TARGET := Agent_test 

all: $(TARGET)

Agent_test: $(AGENT_OBJS) 
	@echo "Building Agent_test..."
	$(CC) $(CFLAGS) $(INCLUDES) $(AGENT_OBJS) -o Agent_test
	@echo "DONE."

Field_test: $(FIELD_OBJS) 
	@echo "Building Field_test..."
	$(CC) $(CFLAGS) $(INCLUDES) $(FIELD_OBJS) -o Field_test
	@echo "DONE."

Message_test: $(MESSAGE_OBJS) 
	@echo "Building Message_test..."
	$(CC) $(CFLAGS) $(INCLUDES) $(MESSAGE_OBJS) -o Message_test
	@echo "DONE."

Negotiation_test: $(NEGOTIATION_OBJS) 
	@echo "Building Negotiation_test..."
	$(CC) $(CFLAGS) $(INCLUDES) $(NEGOTIATION_OBJS) -o Negotiation_test
	@echo "DONE."

# Compilation rule.
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean rule.
clean:
	rm -f $(OBJS) Agent_test Field_test Message_test Negotiation_test

.PHONY: all, clean


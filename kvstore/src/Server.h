#pragma once
#include "Store.h"
#include "Command.h"

bool runServer(Store& store, const CommandTable& commands, int port);
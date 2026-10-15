#pragma once
#include "Command.h"
#include "Store.h"

bool runServer(Store& store, const CommandTable& commands, int port);

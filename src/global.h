#pragma once

#include <base/log.h>
#include <Actor/Actor.h>
#include <Actor/Player.h>
#include <mods/CommandSupport.h>
#include <Item/ItemStack.h>
#include <Item/Item.h>

#include "Enchant.h"
#include "EnchantResult.h"

DEF_LOGGER("GiveNbt");

void initCommand(CommandRegistry *registry);
#pragma once

#include <hook.h>
#include <base/base.h>
#include <base/log.h>
#include <Actor/Actor.h>
#include <Actor/Mob.h>
#include <Actor/Player.h>
#include <mods/CommandSupport.h>
#include <Item/ItemStack.h>
#include <Item/CommandItem.h>
#include <Item/ItemInstance.h>
#include <Item/Enchant.h>
#include <Level/Level.h>
#include <Container/SimpleContainer.h>
#include <Container/Inventory.h>
#include <Container/PlayerInventory.h>
#include <Packet/TextPacket.h>

namespace NBTCommandUtils {

struct Enchantment {
	int16_t id;
	int16_t level;
	Enchantment(int16_t id, int16_t level) : id(id), level(level) {}
	Enchantment() : id(0), level(0) {}
};

bool checkEnchantmentString(std::string &enchantments, CommandOutput &output);
void getEnchantmentsFromString(const char* string, std::vector<Enchantment>& out_split);
void applyUnfilteredEnchant(ItemStackBase &out, EnchantmentInstance const &newEnchant);

}

DEF_LOGGER("GiveNBTCommand");
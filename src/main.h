#pragma once

#include <hook.h>
#include <base/base.h>
#include <base/log.h>
#include <Actor/Player.h>
#include <mods/CommandSupport.h>
#include <Item/ItemStack.h>
#include <Item/Enchant.h>
#include <Item/ItemRegistry.h>
#include <Container/PlayerInventory.h>
#include <Packet/TextPacket.h>

namespace NBTCommandUtils {

struct Enchantment {
	int8_t id;
	int16_t level;

	Enchantment() : id(0), level(0) {}
	Enchantment(int8_t id, int16_t level) : id(id), level(level) {}
};

constexpr inline int8_t MAX_ENCHANT_ID = 36;
constexpr inline int16_t MIN_ENCHANT_TIER = (std::numeric_limits<int16_t>::min)();
constexpr inline int16_t MAX_ENCHANT_TIER = (std::numeric_limits<int16_t>::max)();
constexpr inline int16_t MAX_AUX_LEVEL = (std::numeric_limits<int16_t>::max)();

std::vector<Enchantment> getEnchantmentsFromString(std::string enchStr);
bool isValidEnchantmentString(const std::string &enchStr);
void initializeNBTCommands(CommandRegistry *registry);

} // namespace NBTCommandUtils

class GiveNBTCommand : public Command {
public:

	CommandSelector<Player> selector;
	int32_t id, count, aux;
	bool toEnderChest;
	std::string enchantments;
	std::string name;
	std::string lore1, lore2, lore3, lore4, lore5;

	GiveNBTCommand() : id(0), count(1), aux(0), toEnderChest(false) {
		this->selector.setIncludeDeadPlayers(true);
	}

	virtual void execute(CommandOrigin const &origin, CommandOutput &output) override;
	static void setup(CommandRegistry *registry);
	void giveItem(Player *player, const Item& item, int32_t maxStackSize, bool hasEnchantments,
		bool hasName, bool hasLore, bool sendCommandFeedback);
};

class ReplaceItemNBTCommand : public Command {
public:

	enum class SlotType : int8_t {
		Mainhand       = 0,
		Offhand        = 1,
		Armor          = 2,
		Inventory      = 3,
		Enderchest     = 4,
		CursorSelected = 5
	};

	CommandSelector<Player> selector;
	SlotType type;
	int32_t slotId, id, count, aux;
	std::string enchantments;
	std::string name;
	std::string lore1, lore2, lore3, lore4, lore5;

	ReplaceItemNBTCommand() : type(SlotType::Mainhand), slotId(0), id(0), count(1), aux(0) {
		this->selector.setIncludeDeadPlayers(true);
	}

	virtual void execute(CommandOrigin const &origin, CommandOutput &output) override;
	static void setup(CommandRegistry *registry);
	std::string containerTypeToString(SlotType v);
	void replaceItem(Player *player, ItemStack stack, bool hasEnchantments,
		bool hasName, bool hasLore, bool sendCommandFeedback);
};

DEF_LOGGER("GiveNBTCommand");
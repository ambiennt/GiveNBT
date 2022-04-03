#include "main.h"
#include <dllentry.h>

namespace NBTCommandUtils {

// remove whitespace from enchantment string, check valid characters
// more parsing done in getEnchantmentsFromString
bool checkEnchantmentString(std::string &enchantments, CommandOutput &output) {
    enchantments.erase(std::remove(enchantments.begin(), enchantments.end(), ' '), enchantments.end());
	if (!enchantments.empty()) {
		if (enchantments.find_first_not_of("1234567890:,-") == std::string::npos) return true;
		output.error("The specified enchantment string is invalid and has been ignored");
	}
	return false;
}

void getEnchantmentsFromString(const char* string, std::vector<Enchantment>& out_split) {
	while (string[0] != 0) { // while its not the end of the string
		Enchantment newEnchantment;
		newEnchantment.id = (int16_t)std::clamp(std::atoi(string), 0, 36); // convert to ascii to number, clamp to vanilla enchantment ids
		while ((string[0] != 0) && (string[0] != ':') && (string[0] != ',')) {
			string++;
		}

		if (string[0] == 0) { // make sure to exit before reading stuff that we shouldn't
			newEnchantment.level = 1; // ensure enchantment level always defaults to 1
			out_split.push_back(newEnchantment);
			return;
		}

		else if (string[0] == ',') { // continue parsing!
			newEnchantment.level = 1;
			out_split.push_back(newEnchantment);
			string++;
		}

		else if (string[0] == ':') { // enchantment level
			string++;
			newEnchantment.level = (int16_t)std::clamp(std::atoi(string), -32768, 32767); // convert to ascii to number
			out_split.push_back(newEnchantment);
			while ((string[0] != 0) && (string[0] != ',')) { // continue parsing!
				string++;
			}
			if (string[0] == 0) return;
			else string++;
		}
		/*else if ((string[0] < '0') || (string[0] > '9')) { // letter found
			return;
		}*/
		else return; // very invalid string
	}
}

} // namespace NBTCommandUtils

class GiveNbtCommand : public Command {
public:

	GiveNbtCommand() {
		selector.setIncludeDeadPlayers(true);
	}

	//command arguments, also initialize values
	CommandSelector<Player> selector;
	int32_t id = 0;
	int32_t count = 1;
	int32_t aux = 0;
	bool toEnderChest = false;
	std::string enchantments;
	std::string name;
	std::string lore1, lore2, lore3, lore4, lore5;

	void giveItem(Player *player, CommandItem &cmi, ItemStack &item,
		bool hasEnchantments, bool hasName, bool hasLore, bool sendCommandFeedback) {

		// loop through count to give more than 1 stack of the item
		const int32_t playerInventorySlots = 36;
		int32_t maxStackSize = (int32_t)item.getMaxStackSize();
		int32_t countNew = (int32_t)std::min(this->count, maxStackSize * playerInventorySlots);

		while (countNew > 0) {

			int32_t currentStack = (int32_t)std::min(maxStackSize, countNew);
			cmi.createInstance(&item, currentStack, this->aux, false);
			countNew -= currentStack;

			// apply NBT to created item
			if (hasEnchantments) {

				EnchantmentInstance instance;
				std::vector<NBTCommandUtils::Enchantment> enchantmentsVector;
				NBTCommandUtils::getEnchantmentsFromString(this->enchantments.c_str(), enchantmentsVector);

				// loop through results of getEnchantmentsFromString and apply to item instance
				for (auto& enchant : enchantmentsVector) {
					instance.type = (Enchant::Type)enchant.id; // type of enchant
					instance.level = enchant.level; // tier of enchant
					EnchantUtils::applyEnchant(item, instance, true);
				}
			}

			if (hasName) {
				item.setCustomName(this->name);
			}

			if (hasLore) {
				std::vector<std::string> loreVector = { this->lore1, this->lore2, this->lore3, this->lore4, this->lore5 };

				// remove empty strings from right to left, break loop after iterating through a non-empty string
				while ((loreVector.size() > 0) && loreVector.back().empty()) {
					loreVector.pop_back();
				}
				item.setCustomLore(loreVector);
			}

			if (this->toEnderChest) {
				auto enderChest = player->getEnderChestContainer();
				if (!enderChest) return;
				enderChest->add(item);
			}
			else {
				// drop item on ground if inventory is full
				if (!player->add(item)) {
					player->drop(item, false);
				}
			}
		}

		if (!this->toEnderChest) {
			player->sendInventory(false); // update inventory to client
		}

		if (sendCommandFeedback) {
			auto receivedItemMessage = TextPacket::createTextPacket<TextPacketType::SystemMessage>(
				"§a[GiveNBT]§r You have been given * " + std::to_string(this->count) + " of item with ID " +
				std::to_string(this->id) + ":" + std::to_string(this->aux));
			player->sendNetworkPacket(receivedItemMessage);
		}
	}

	void execute(CommandOrigin const &origin, CommandOutput &output) {

		// get selected entity/entities
		auto selectedEntities = selector.results(origin);
		if (selectedEntities.empty()) {
			//output.error("commands.generic.noTargetMatch");
			return output.error("No targets matched selector");
		}

		// hack: create empty instance of an item to test ID and stack size
		// using id and aux from in-game command parameters
		// we need to pass in the aux because different auxes have different stack sizes
		CommandItem cmi(this->id);
		ItemStack testItem;
		cmi.createInstance(&testItem, 0, this->aux, false);
		if (testItem.isNull()) {
			return output.error("Specified item ID is invalid");
		}

		std::string countStr = std::to_string(this->count);
		std::string auxStr   = std::to_string(this->aux);

		if ((this->count < 0) || (this->count > 32767)) {
			return output.error("The count you have entered (" + countStr + ") is not within the allowed range of 0-32767");
		}
		if ((this->aux < 0) || (this->aux > 32767)) {
			return output.error("The aux value you have entered (" + auxStr + ") is not within the allowed range of 0-32767");
		}

		bool hasEnchantments = NBTCommandUtils::checkEnchantmentString(this->enchantments, output);

		bool hasName = !this->name.empty();

		std::array<std::string, 5> loreArr = { this->lore1, this->lore2, this->lore3, this->lore4, this->lore5 };
		bool hasLore = false;
		for (int32_t i = 0; i < loreArr.size(); i++) {
			if (!loreArr[i].empty()) {
				hasLore = true;
			}
		}

		bool sendCommandFeedback = (output.type != CommandOutputType::NoFeedback);

		// loop through selector results
		// pass in parameter data to prevent unnecessary checks for each selected entity
		for (auto player : selectedEntities) {
			giveItem(player, cmi, testItem, hasEnchantments, hasName, hasLore, sendCommandFeedback);
		}
		output.success(
			"§a[GiveNBT]§r Gave * " + countStr + " of item with ID " + std::to_string(this->id) + ":" + auxStr +
			" to " + std::to_string(selectedEntities.count()) + (selectedEntities.count() == 1 ? " player" : " players"));
	}

	static void setup(CommandRegistry *registry) {
		using namespace commands;

		registry->registerCommand(
			"givenbt", "Gives an item with custom NBT.",
			CommandPermissionLevel::GameMasters, CommandFlagUsage, CommandFlagNone);

		registry->registerOverload<GiveNbtCommand>("givenbt",
			mandatory(&GiveNbtCommand::selector, "player"),
			mandatory(&GiveNbtCommand::id, "itemId"),
			optional(&GiveNbtCommand::count, "count"),
			optional(&GiveNbtCommand::aux, "auxValue"),
			optional(&GiveNbtCommand::toEnderChest, "toEnderChest"),
			optional(&GiveNbtCommand::enchantments, "enchantments"),
			optional(&GiveNbtCommand::name, "customName"),
			optional(&GiveNbtCommand::lore1, "lore1"),
			optional(&GiveNbtCommand::lore2, "lore2"),
			optional(&GiveNbtCommand::lore3, "lore3"),
			optional(&GiveNbtCommand::lore4, "lore4"),
			optional(&GiveNbtCommand::lore5, "lore5")
		);
	}
};

class ReplaceItemNbtCommand : public Command {
public:

	ReplaceItemNbtCommand() {
		selector.setIncludeDeadPlayers(true);
	}

	enum class SlotType {
		Mainhand       = 0,
		Offhand        = 1,
		Armor          = 2,
		Inventory      = 3,
		Enderchest     = 4,
		CursorSelected = 5
	};

	CommandSelector<Player> selector;
	SlotType type;
	int32_t slotId = 0;
	int32_t id = 0;
	int32_t count = 1;
	int32_t aux = 0;
	std::string enchantments;
	std::string name;
	std::string lore1, lore2, lore3, lore4, lore5;

	constexpr const char* containerTypeToString(SlotType v) {
		switch (v) {
			case SlotType::Mainhand:        return "Mainhand";
			case SlotType::Offhand:         return "Offhand";
			case SlotType::Armor:           return "Armor";
			case SlotType::Inventory:       return "Inventory";
			case SlotType::Enderchest:      return "Enderchest";
			case SlotType::CursorSelected:  return "CursorSelected";
			default:                        return "Unknown";
		}
	}

	void replaceItem(Player *player, CommandItem &cmi, ItemStack &item,
		bool hasEnchantments, bool hasName, bool hasLore, bool sendCommandFeedback) {

		cmi.createInstance(&item, this->count, this->aux, false);

		if (hasEnchantments) {

			EnchantmentInstance instance;
			std::vector<NBTCommandUtils::Enchantment> enchantmentsVector;
			NBTCommandUtils::getEnchantmentsFromString(this->enchantments.c_str(), enchantmentsVector);

			for (auto& enchant : enchantmentsVector) {
				instance.type = (Enchant::Type)enchant.id;
				instance.level = enchant.level;
				EnchantUtils::applyEnchant(item, instance, true);
			}
		}

		if (hasName) {
			item.setCustomName(this->name);
		}

		if (hasLore) {
			std::vector<std::string> loreVector = { this->lore1, this->lore2, this->lore3, this->lore4, this->lore5 };
			while ((loreVector.size() > 0) && loreVector.back().empty()) {
				loreVector.pop_back();
			}
			item.setCustomLore(loreVector);
		}

		switch (this->type) {
			case SlotType::Mainhand:
				player->setSelectedItem(item);
				break;

			case SlotType::Offhand:
				player->setOffhandSlot(item);
				break;

			case SlotType::Armor:
				player->setArmor((ArmorSlot)this->slotId, item);
				break;

			case SlotType::Inventory: // hotbar and inventory - slot ids 0-8 and 9-35 respectively
				player->getRawInventory().setItem(this->slotId, item);
				break;

			case SlotType::Enderchest: {
				auto enderChest = player->getEnderChestContainer();
				if (!enderChest) return;
				enderChest->setItem(this->slotId, item);
				break;
			}

			case SlotType::CursorSelected:
				player->setPlayerUIItem((PlayerUISlot)this->slotId, item);
				break;

			default: break;
		}

		if (this->type != SlotType::Enderchest) {
			player->sendInventory(false);
		}

		if (sendCommandFeedback) {
			auto receivedItemMessage = TextPacket::createTextPacket<TextPacketType::SystemMessage>(
				"§a[ReplaceItemNBT]§r You have been set with * " + std::to_string(this->count) + " of item with ID " +
				std::to_string(this->id) + ":" + std::to_string(this->aux) + " in slot " + containerTypeToString(this->type) +
				":" + std::to_string(this->slotId));
			player->sendNetworkPacket(receivedItemMessage);
		}
	}

	void execute(CommandOrigin const &origin, CommandOutput &output) {

		auto selectedEntities = selector.results(origin);
		if (selectedEntities.empty()) {
			return output.error("No targets matched selector");
		}

		CommandItem cmi(this->id);
		ItemStack testItem;
		cmi.createInstance(&testItem, 0, this->aux, false);
		if (testItem.isNull()) {
			return output.error("Specified item ID is invalid");
		}

		std::string slotIdStr = std::to_string(this->slotId);
		std::string idStr     = std::to_string(this->id);
		std::string countStr  = std::to_string(this->count);
		std::string auxStr    = std::to_string(this->aux);

		std::string slodIdErrStr = "The slot ID you have entered (" + slotIdStr + ") ";
		switch (type) {
			case SlotType::Mainhand:
			case SlotType::Offhand:
			case SlotType::CursorSelected:
				if (this->slotId != 0) {
					return output.error(slodIdErrStr + "must be exactly 0");
				}
				break;

			case SlotType::Armor:
				if ((this->slotId < 0) || (this->slotId > 3)) {
					return output.error(slodIdErrStr + "is not within the allowed range of 0-3");
				}
				break;

			case SlotType::Inventory:
				if ((this->slotId < 0) || (this->slotId > 35)) {
					return output.error(slodIdErrStr + "is not within the allowed range of 0-35");
				}
				break;

			case SlotType::Enderchest:
				if ((this->slotId < 0) || (this->slotId > 26)) {
					return output.error(slodIdErrStr + "is not within the allowed range of 0-26");
				}
				break;

			default: break;
		}

		int32_t maxStackSize = (int32_t)testItem.getMaxStackSize();

		if ((this->count < 1) || (this->count > maxStackSize)) {
			return output.error("The count you have entered (" + countStr + ") is not within the allowed range of 1-" + std::to_string(maxStackSize));
		}
		if ((this->aux < 0) || (this->aux > 32767)) {
			return output.error("The aux value you have entered (" + auxStr + ") is not within the allowed range of 0-32767");
		}

		bool hasEnchantments = NBTCommandUtils::checkEnchantmentString(this->enchantments, output);

		bool hasName = !this->name.empty();

		std::array<std::string, 5> loreArr = { this->lore1, this->lore2, this->lore3, this->lore4, this->lore5 };
		bool hasLore = false;
		for (int32_t i = 0; i < loreArr.size(); i++) {
			if (!loreArr[i].empty()) {
				hasLore = true;
			}
		}
		
		bool sendCommandFeedback = (output.type != CommandOutputType::NoFeedback);

		for (auto player : selectedEntities) {
			replaceItem(player, cmi, testItem, hasEnchantments, hasName, hasLore, sendCommandFeedback);
		}
		output.success(
			"§a[ReplaceItemNBT]§r Set * " + countStr + " of item with ID " + idStr + ":" +
			auxStr + " in slot " + containerTypeToString(type) + ":" + slotIdStr + " for " +
			std::to_string(selectedEntities.count()) + (selectedEntities.count() == 1 ? " player" : " players"));
	}

	static void setup(CommandRegistry *registry) {
		using namespace commands;

		registry->registerCommand(
			"replaceitemnbt", "Sets an item with custom NBT in the specified equipment slot.",
			CommandPermissionLevel::GameMasters, CommandFlagUsage, CommandFlagNone);

		addEnum<SlotType>(registry, "slotType", {
			{ "slot.weapon.mainhand", SlotType::Mainhand },
			{ "slot.weapon.offhand", SlotType::Offhand },
			{ "slot.armor", SlotType::Armor },
			{ "slot.inventory", SlotType::Inventory },
			{ "slot.enderchest", SlotType::Enderchest },
			{ "slot.ui.cursorselected", SlotType::CursorSelected }
		});

		registry->registerOverload<ReplaceItemNbtCommand>("replaceitemnbt",
			mandatory(&ReplaceItemNbtCommand::selector, "player"),
			mandatory<CommandParameterDataType::ENUM>(&ReplaceItemNbtCommand::type, "type", "slotType"),
			mandatory(&ReplaceItemNbtCommand::slotId, "slotId"),
			mandatory(&ReplaceItemNbtCommand::id, "itemId"),
			optional(&ReplaceItemNbtCommand::count, "count"),
			optional(&ReplaceItemNbtCommand::aux, "auxValue"),
			optional(&ReplaceItemNbtCommand::enchantments, "enchantments"),
			optional(&ReplaceItemNbtCommand::name, "customName"),
			optional(&ReplaceItemNbtCommand::lore1, "lore1"),
			optional(&ReplaceItemNbtCommand::lore2, "lore2"),
			optional(&ReplaceItemNbtCommand::lore3, "lore3"),
			optional(&ReplaceItemNbtCommand::lore4, "lore4"),
			optional(&ReplaceItemNbtCommand::lore5, "lore5")
		);
	}
};

void dllenter() {}
void dllexit() {}
void PreInit() {
	Mod::CommandSupport::GetInstance().AddListener(SIG("loaded"), &GiveNbtCommand::setup);
	Mod::CommandSupport::GetInstance().AddListener(SIG("loaded"), &ReplaceItemNbtCommand::setup);
}
void PostInit() {}
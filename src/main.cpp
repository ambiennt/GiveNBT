#include "main.h"
#include <dllentry.h>

void dllenter() {}
void dllexit() {}
void PreInit() {
	Mod::CommandSupport::GetInstance().AddListener(SIG("loaded"), NBTCommandUtils::initializeNBTCommands);
}
void PostInit() {}

void NBTCommandUtils::initializeNBTCommands(CommandRegistry *registry) {
	GiveNBTCommand::setup(registry);
	ReplaceItemNBTCommand::setup(registry);
}

// soft-validation
bool NBTCommandUtils::isValidEnchantmentString(const std::string &enchStr) {
	return (enchStr.find_first_not_of("1234567890:,- ") == std::string::npos);
}

std::vector<NBTCommandUtils::Enchantment> NBTCommandUtils::getEnchantmentsFromString(std::string enchStr) {

	enchStr.erase(std::remove(enchStr.begin(), enchStr.end(), ' '), enchStr.end()); // remove extra spaces...
	const char* rawStr = enchStr.c_str();

	std::vector<Enchantment> result{};

	while (rawStr[0] != 0) { // while its not the end of the string

		Enchantment newEnchantment{};
		newEnchantment.id = std::clamp((int8_t)std::atoi(rawStr), (int8_t)0, MAX_ENCHANT_ID); // convert to ascii to number, clamp to vanilla enchantment ids
		while ((rawStr[0] != 0) && (rawStr[0] != ':') && (rawStr[0] != ',')) {
			rawStr++;
		}

		if (rawStr[0] == 0) { // make sure to exit before reading stuff that we shouldn't
			newEnchantment.level = 1; // ensure enchantment level always defaults to 1
			result.push_back(newEnchantment);
			break;
		}
		else if (rawStr[0] == ',') { // continue parsing!
			newEnchantment.level = 1;
			result.push_back(newEnchantment);
			rawStr++;
		}
		else if (rawStr[0] == ':') { // enchantment level
			rawStr++;
			newEnchantment.level = std::clamp((int16_t)std::atoi(rawStr), MIN_ENCHANT_TIER, MAX_ENCHANT_TIER); // convert to ascii to number
			result.push_back(newEnchantment);
			while ((rawStr[0] != 0) && (rawStr[0] != ',')) { // continue parsing!
				rawStr++;
			}
			if (rawStr[0] == 0) break;
			else rawStr++;
		}
		/*else if ((rawStr[0] < '0') || (rawStr[0] > '9')) { // letter found
			return;
		}*/
		else break; // very invalid string
	}
	return result;
}















void GiveNBTCommand::giveItem(Player *player, const Item& item, int32_t maxStackSize,
	bool hasEnchantments, bool hasName, bool hasLore, bool sendCommandFeedback) {

	// loop through count to give more than 1 stack of the item
	int32_t containerSize = 0;
	if (this->toEnderChest) {
		auto enderChest = player->getEnderChestContainer();
		if (!enderChest) return;
		containerSize = enderChest->getContainerSize();
	}
	else {
		containerSize = player->getRawInventory().getContainerSize();
	}
	int32_t countNew = std::min(this->count, maxStackSize * containerSize);

	while (countNew > 0) {

		int32_t currentStackSize = (int32_t)std::min(maxStackSize, countNew);
		ItemStack stack(item, currentStackSize, this->aux);
		countNew -= currentStackSize;

		// apply NBT to created ItemStack
		if (hasEnchantments) {
			auto enchantmentsVector = NBTCommandUtils::getEnchantmentsFromString(this->enchantments);

			// loop through results of getEnchantmentsFromString and apply to item instance
			for (auto& enchant : enchantmentsVector) {
				EnchantmentInstance instance((Enchant::Type)enchant.id, enchant.level); // tier and type of enchant
				EnchantUtils::applyUnfilteredEnchant(stack, instance, false);
			}
		}

		if (hasName) {
			stack.setCustomName(this->name);
		}

		if (hasLore) {
			std::vector<std::string> loreVector = { this->lore1, this->lore2, this->lore3, this->lore4, this->lore5 };

			// remove empty strings from right to left, break loop after iterating through a non-empty string
			while ((loreVector.size() > 0) && loreVector.back().empty()) {
				loreVector.pop_back();
			}
			stack.setCustomLore(loreVector);
		}

		if (this->toEnderChest) {
			auto enderChest = player->getEnderChestContainer();
			if (!enderChest) return;
			enderChest->add(stack);
		}
		else {
			// drop ItemStack on ground if inventory is full
			if (!player->add(stack)) {
				player->drop(stack, false);
			}
		}
	}

	if (!this->toEnderChest) {
		player->sendInventory(false); // update inventory to client
	}

	if (sendCommandFeedback) {
		auto receivedItemMessage = TextPacket::createTextPacket<TextPacketType::SystemMessage>(
			"§a[GiveNBT]§r " + (this->toEnderChest ? std::string("Your ender chest has ") : std::string("You have ")) + "been given * " +
			std::to_string(this->count) + " of item with ID " + std::to_string(this->id) + ":" + std::to_string(this->aux));
		player->sendNetworkPacket(receivedItemMessage);
	}
}

void GiveNBTCommand::execute(CommandOrigin const &origin, CommandOutput &output) {

	// get selected entity/entities
	auto selectedEntities = selector.results(origin);
	if (selectedEntities.empty()) {
		//output.error("commands.generic.noTargetMatch");
		return output.error("No targets matched selector");
	}

	// hack: create empty instance of an item to test ID and stack size
	// using id and aux from in-game command parameters
	// we need to pass in the aux because different auxes have different stack sizes
	std::string invalidItemStr("Specified item ID is invalid");
	auto item = ItemRegistry::getItem(this->id);
	if (!item) {
		output.error(invalidItemStr);
		return;
	}
	ItemStack testStack(*item, 1, this->aux);
	if (testStack.isNull()) {
		output.error(invalidItemStr);
		return;
	}

	std::string countStr = std::to_string(this->count);
	std::string auxStr   = std::to_string(this->aux);

	if (this->count < 1) {
		output.error("The count you have entered (" + countStr + ") must be at least 1");
		return;
	}
	if ((this->aux < 0) || (this->aux > NBTCommandUtils::MAX_AUX_LEVEL)) {
		output.error(
			"The aux value you have entered (" + auxStr + ") is not within the allowed range of 0-" + std::to_string(NBTCommandUtils::MAX_AUX_LEVEL));
		return;
	}

	bool hasEnchantments = NBTCommandUtils::isValidEnchantmentString(this->enchantments);
	if (!this->enchantments.empty() && !hasEnchantments) {
		output.error("The specified enchantment string is invalid and has been ignored");
	}
	bool hasName = !this->name.empty();
	bool hasLore = (!this->lore1.empty() || !this->lore2.empty() || !this->lore3.empty() || !this->lore4.empty() || !this->lore5.empty());
	bool sendCommandFeedback = (output.type != CommandOutputType::NoFeedback);

	// loop through selector results
	// pass in parameter data to prevent unnecessary checks for each selected entity
	for (auto player : selectedEntities) {
		giveItem(player, *item, (int32_t)testStack.getMaxStackSize(), hasEnchantments, hasName, hasLore, sendCommandFeedback);
	}
	output.success(
		"§a[GiveNBT]§r Gave * " + countStr + " of item with ID " + std::to_string(this->id) + ":" + auxStr +
		" to " + std::to_string(selectedEntities.count()) + ((selectedEntities.count() == 1) ? " player" : " players"));
}

void GiveNBTCommand::setup(CommandRegistry *registry) {
	using namespace commands;

	registry->registerCommand(
		"givenbt", "Gives an item with custom NBT.",
		CommandPermissionLevel::GameMasters, CommandFlagUsage, CommandFlagNone);

	registry->registerOverload<GiveNBTCommand>("givenbt",
		mandatory(&GiveNBTCommand::selector, "player"),
		mandatory(&GiveNBTCommand::id, "itemId"),
		optional(&GiveNBTCommand::count, "count"),
		optional(&GiveNBTCommand::aux, "auxValue"),
		optional(&GiveNBTCommand::toEnderChest, "toEnderChest"),
		optional(&GiveNBTCommand::enchantments, "enchantments"),
		optional(&GiveNBTCommand::name, "customName"),
		optional(&GiveNBTCommand::lore1, "lore1"),
		optional(&GiveNBTCommand::lore2, "lore2"),
		optional(&GiveNBTCommand::lore3, "lore3"),
		optional(&GiveNBTCommand::lore4, "lore4"),
		optional(&GiveNBTCommand::lore5, "lore5")
	);
}















std::string ReplaceItemNBTCommand::containerTypeToString(SlotType v) {
	switch (v) {
		case SlotType::Mainhand:        return std::string("Mainhand");
		case SlotType::Offhand:         return std::string("Offhand");
		case SlotType::Armor:           return std::string("Armor");
		case SlotType::Inventory:       return std::string("Inventory");
		case SlotType::Enderchest:      return std::string("Enderchest");
		case SlotType::CursorSelected:  return std::string("CursorSelected");
		default:                        return std::string("Unknown");
	}
}

void ReplaceItemNBTCommand::replaceItem(Player *player, ItemStack stack,
	bool hasEnchantments, bool hasName, bool hasLore, bool sendCommandFeedback) {

	if (hasEnchantments) {

		auto enchantmentsVector = NBTCommandUtils::getEnchantmentsFromString(this->enchantments);

		for (auto& enchant : enchantmentsVector) {
			EnchantmentInstance instance((Enchant::Type)enchant.id, enchant.level);
			EnchantUtils::applyUnfilteredEnchant(stack, instance, false);
		}
	}

	if (hasName) {
		stack.setCustomName(this->name);
	}

	if (hasLore) {
		std::vector<std::string> loreVector = { this->lore1, this->lore2, this->lore3, this->lore4, this->lore5 };
		while ((loreVector.size() > 0) && loreVector.back().empty()) {
			loreVector.pop_back();
		}
		stack.setCustomLore(loreVector);
	}

	switch (this->type) {
		case SlotType::Mainhand:
			player->setSelectedItem(stack);
			break;

		case SlotType::Offhand:
			player->setOffhandSlot(stack);
			break;

		case SlotType::Armor:
			player->setArmor((ArmorSlot)this->slotId, stack);
			break;

		case SlotType::Inventory: // hotbar and inventory - slot ids 0-8 and 9-35 respectively
			player->getRawInventory().setItem(this->slotId, stack);
			break;

		case SlotType::Enderchest: {
			auto enderChest = player->getEnderChestContainer();
			if (!enderChest) return;
			enderChest->setItem(this->slotId, stack);
			break;
		}

		case SlotType::CursorSelected:
			player->setPlayerUIItem((PlayerUISlot)this->slotId, stack);
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

void ReplaceItemNBTCommand::execute(CommandOrigin const &origin, CommandOutput &output) {

	auto selectedEntities = selector.results(origin);
	if (selectedEntities.empty()) {
		return output.error("No targets matched selector");
	}

	std::string slotIdStr = std::to_string(this->slotId);
	std::string idStr     = std::to_string(this->id);
	std::string countStr  = std::to_string(this->count);
	std::string auxStr    = std::to_string(this->aux);

	std::string slodIdErrStr("The slot ID you have entered (" + slotIdStr + ") ");
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

	std::string invalidItemStr("Specified item ID is invalid");
	auto item = ItemRegistry::getItem(this->id);
	if (!item) {
		output.error(invalidItemStr);
		return;
	}
	ItemStack testStack(*item, 1, this->aux);
	if (testStack.isNull()) {
		output.error(invalidItemStr);
		return;
	}

	int32_t maxStackSize = (int32_t)testStack.getMaxStackSize();

	if ((this->count < 1) || (this->count > maxStackSize)) {
		output.error(
			"The count you have entered (" + countStr + ") is not within the allowed range of 1-" + std::to_string(maxStackSize));
		return;
	}
	if ((this->aux < 0) || (this->aux > NBTCommandUtils::MAX_AUX_LEVEL)) {
		output.error(
			"The aux value you have entered (" + auxStr + ") is not within the allowed range of 0-" + std::to_string(NBTCommandUtils::MAX_AUX_LEVEL));
		return;

	}

	bool hasEnchantments = NBTCommandUtils::isValidEnchantmentString(this->enchantments);
	if (!this->enchantments.empty() && !hasEnchantments) {
		output.error("The specified enchantment string is invalid and has been ignored");
	}
	bool hasName = !this->name.empty();
	bool hasLore = (!this->lore1.empty() || !this->lore2.empty() || !this->lore3.empty() || !this->lore4.empty() || !this->lore5.empty());
	bool sendCommandFeedback = (output.type != CommandOutputType::NoFeedback);

	for (auto player : selectedEntities) {
		replaceItem(player, ItemStack(*item, this->count, this->aux), hasEnchantments, hasName, hasLore, sendCommandFeedback);
	}

	output.success(
		"§a[ReplaceItemNBT]§r Set * " + countStr + " of item with ID " + idStr + ":" +
		auxStr + " in slot " + containerTypeToString(type) + ":" + slotIdStr + " for " +
		std::to_string(selectedEntities.count()) + ((selectedEntities.count() == 1) ? " player" : " players"));
}

void ReplaceItemNBTCommand::setup(CommandRegistry *registry) {
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

	registry->registerOverload<ReplaceItemNBTCommand>("replaceitemnbt",
		mandatory(&ReplaceItemNBTCommand::selector, "player"),
		mandatory<CommandParameterDataType::ENUM>(&ReplaceItemNBTCommand::type, "type", "slotType"),
		mandatory(&ReplaceItemNBTCommand::slotId, "slotId"),
		mandatory(&ReplaceItemNBTCommand::id, "itemId"),
		optional(&ReplaceItemNBTCommand::count, "count"),
		optional(&ReplaceItemNBTCommand::aux, "auxValue"),
		optional(&ReplaceItemNBTCommand::enchantments, "enchantments"),
		optional(&ReplaceItemNBTCommand::name, "customName"),
		optional(&ReplaceItemNBTCommand::lore1, "lore1"),
		optional(&ReplaceItemNBTCommand::lore2, "lore2"),
		optional(&ReplaceItemNBTCommand::lore3, "lore3"),
		optional(&ReplaceItemNBTCommand::lore4, "lore4"),
		optional(&ReplaceItemNBTCommand::lore5, "lore5")
	);
}
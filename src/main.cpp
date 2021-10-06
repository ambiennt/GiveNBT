#include "main.h"
#include <dllentry.h>

#define FWD(...) \
    std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)

#define LIFT(...) \
    [](auto&&... args) \
        noexcept(noexcept(__VA_ARGS__(FWD(args)...)))  \
        -> decltype(__VA_ARGS__(FWD(args)...)) \
    { return __VA_ARGS__(FWD(args)...); }

void PreInit() {}
void PostInit() {}
void dllenter() {}
void dllexit() {}

bool fromNbtCommand = false;

class CommandItem {
public:
    int version = 0;
    int id = 0;
    ItemStackBase *createInstance(ItemStackBase *is, int count, int aux, CommandOutput &output, bool exactAux) {
        return CallServerClassMethod<ItemStackBase *>(
            "?createInstance@CommandItem@@QEBA?AV?$optional@VItemInstance@@@std@@HHPEAVCommandOutput@@_N@Z", this, is,
        count, aux, &output, exactAux);
    }
};

struct Enchantment {
    short id;
    short level;
    Enchantment(short id, short level) : id(id), level(level) {}
    Enchantment() : id(0), level(0) {}
};

inline bool getBoolFromGameruleId(const int id) {
    auto gr = CallServerClassMethod<GameRules*>("?getGameRules@Level@@QEAAAEAVGameRules@@XZ", LocateService<Level>());
    return CallServerClassMethod<bool>("?getBool@GameRules@@QEBA_NUGameRuleId@@@Z", gr, &id);
}

inline bool dropItem(Player *player, ItemStack const &is, bool random) {
    return CallServerClassMethod<bool>("?drop@Player@@UEAA_NAEBVItemStack@@_N@Z", player, &is, false);
}

inline bool addItem(Player *player, ItemStack &is) {
    return CallServerClassMethod<bool>("?add@Player@@UEAA_NAEAVItemStack@@@Z", player, &is);
}

bool checkEnchantmentString(std::string &enchantments, CommandOutput &output) {

    //remove whitespace from enchantment string, check valid characters
    //more parsing done in getEnchantmentsFromString
    enchantments.erase(std::remove(enchantments.begin(), enchantments.end(), ' '), enchantments.end());
    if (!enchantments.empty()) {
        if (enchantments.find_first_not_of("1234567890:,-") == std::string::npos) return true;
        output.error("The specified enchantment string is invalid and has been ignored");
    }
    return false;
}

void getEnchantmentsFromString(const char* string, std::vector<Enchantment>& out_split) {
    while (string[0] != 0) { //while its not the end of the string
        Enchantment newEnchantment;
        newEnchantment.id = std::clamp(std::atoi(string), 0, 36); //convert to ascii to number, clamp to vanilla enchantment ids
        while (string[0] != 0 && string[0] != ':' && string[0] != ',') {
            string++;
        }

        if (string[0] == 0) { //make sure to exit before reading stuff that we shouldn't
            newEnchantment.level = 1; //ensure enchantment level always defaults to 1
            out_split.push_back(newEnchantment);
            return;
        }

        else if (string[0] == ',') { //continue parsing!
            newEnchantment.level = 1;
            out_split.push_back(newEnchantment);
            string++;
        }

        else if (string[0] == ':') { //Enchantment level
            string++;
            newEnchantment.level = std::clamp(std::atoi(string), -32768, 32767); //convert to ascii to number
            out_split.push_back(newEnchantment);
            while (string[0] != 0 && string[0] != ',') { //continue parsing!
                string++;
            }
            if (string[0] == 0) return;
            else string++;
        }
        /*else if (string[0] < '0' || string[0] > '9')
            return; //letter found, return*/
        else return; //very invalid string
    }
}

THook(EnchantResult*, "?canEnchant@ItemEnchants@@QEAA?AUEnchantResult@@VEnchantmentInstance@@_N@Z",
    ItemEnchants *self, EnchantmentInstance enchant, bool allowNonVanilla) {
    auto ret = original(self, enchant, allowNonVanilla);
    if (fromNbtCommand) {
        ret->result = EnchantResultType::Enchant; // force enchant
    }
    return ret;
}

class GiveNbtCommand : public Command {
public:
    GiveNbtCommand() { selector.setIncludeDeadPlayers(true); }

    //command arguments, also initialize values
    CommandSelector<Player> selector;
    int id = 0;
    int count = 1;
    int aux = 0;
    bool toEnderChest = false;
    std::string enchantments;
    std::string name;
    std::string lore1;
    std::string lore2;
    std::string lore3;
    std::string lore4;
    std::string lore5;

    void giveItem(Player *player, CommandOutput &output, CommandItem &cmi, ItemStack &is,
        bool hasEnchantments, bool hasName, bool hasLore, bool sendCommandFeedback) {

        //loop through count to give more than 1 stack of the item
        const int playerInventorySlots = 36;
        int maxStackSize = is.getMaxStackSize();
        int countNew = count;
        countNew = std::min(countNew, maxStackSize * playerInventorySlots);
        
        auto playerEnderChest = player->getEnderChestContainer();

        while (countNew > 0) {
            int this_stack = std::min(maxStackSize, countNew);
            cmi.createInstance(&is, this_stack, aux, output, false);
            countNew -= this_stack;

            //apply NBT to created item
            if (hasEnchantments) {
                EnchantmentInstance instance;
                std::vector<Enchantment> enchantmentsVector;
                getEnchantmentsFromString(enchantments.c_str(), enchantmentsVector);
                fromNbtCommand = true;

                //loop through results of getEnchantmentsFromString and apply to item instance
                for (auto& enchant : enchantmentsVector) {
                    //std::cout << "ID: " << enchant.id << "   Level: " << enchant.level << std::endl;
                    instance.type = (Enchant::Type) enchant.id; //type of enchant
                    instance.level = enchant.level; //tier of enchant
                    EnchantUtils::applyEnchant(is, instance, true);
                }
                fromNbtCommand = false;
            }

            if (hasName) {
                is.setCustomName(name);
            }

            if (hasLore) {
                std::vector<std::string> loreVector = { lore1, lore2, lore3, lore4, lore5 };

                //remove empty strings from right to left, break loop after iterating through a non-empty string
                while (loreVector.size() > 0 && loreVector.back().empty()) {
                    loreVector.pop_back();
                }
                is.setCustomLore(loreVector);
            }

            if (toEnderChest) {
                playerEnderChest->addItemToFirstEmptySlot(is);
            }
            else {
                //drop item if inventory is full
                if (!addItem(player, is)) {
                    dropItem(player, is, false);
                }
            }
        }

        CallServerClassMethod<void>("?sendInventory@ServerPlayer@@UEAAX_N@Z", player, false);

        if (sendCommandFeedback) {
            /*output.success(
                "§a[GiveNBT]§r You have been given * " + std::to_string(count) + " of item with ID " + std::to_string(id) + ":" + std::to_string(aux));*/
            auto receivedItemMessage = TextPacket::createTextPacket<TextPacketType::SystemMessage>(
                "§a[GiveNBT]§r You have been given * " + std::to_string(count) + " of item with ID " + std::to_string(id) + ":" + std::to_string(aux));
            player->sendNetworkPacket(receivedItemMessage);
        }
    }

    void execute(CommandOrigin const &origin, CommandOutput &output) {

        std::string stringifiedCount = std::to_string(count);
        std::string stringifiedAux = std::to_string(aux);
        if (count < 0 || count > 32767) {
            return output.error("The count you have entered (" + stringifiedCount + ") is not within the allowed range of 0-32767");
        }
        else if (aux < 0 || aux > 32767) {
            return output.error("The aux value you have entered (" + stringifiedAux + ") is not within the allowed range of 0-32767");
        }

        //get selected entity/entities
        auto selectedEntities = selector.results(origin);
        if (selectedEntities.empty()) {
            //output.error("commands.generic.noTargetMatch");
            return output.error("No targets matched selector");
        }

        //hack: create empty instance of an item to test ID and stack size
        //using id and aux from in-game command parameters
        CommandItem cmi;
        cmi.id = id;
        ItemStack is;
        cmi.createInstance(&is, 0, aux, output, false);
        if (is.isNull()) {
            return output.error("Specified item ID is invalid");
        }

        bool hasEnchantments = checkEnchantmentString(enchantments, output);
        bool hasName = !name.empty();
        //bool hasLore = !lore1.empty() || !lore2.empty() || !lore3.empty(); || !lore4.empty(); || !lore5.empty();
        std::string lore[5] = { lore1, lore2, lore3, lore4, lore5 };
        bool hasLore = !std::all_of(lore, std::end(lore), LIFT(std::empty));
        bool sendCommandFeedback = getBoolFromGameruleId(17);

        //loop through selector results
        //pass in parameter data to prevent unnecessary checks for each selected entity
        for (auto player : selectedEntities) {
            giveItem(player, output, cmi, is, hasEnchantments, hasName, hasLore, sendCommandFeedback);
        }
        output.success(
            "§a[GiveNBT]§r Gave * " + stringifiedCount + " of item with ID " + std::to_string(id) + ":" + stringifiedAux + " to " + std::to_string(selectedEntities.count()) + (selectedEntities.count() == 1 ? " player" : " players"));
    }

    static void setup(CommandRegistry *registry) {
        using namespace commands;
        registry->registerCommand(
            "givenbt", "Gives an item with custom NBT.", CommandPermissionLevel::GameMasters, CommandFlagCheat, CommandFlagNone);
        registry->registerOverload<GiveNbtCommand>("givenbt",
            mandatory(&GiveNbtCommand::selector, "target"),
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
    ReplaceItemNbtCommand() { selector.setIncludeDeadPlayers(true); }

    CommandSelector<Player> selector;
    enum class SlotType {
        Mainhand    = 0,
        Offhand     = 1,
        Armor       = 2,
        Inventory   = 3,
        Enderchest  = 4
    } type;
    int slotId = 0;
    int id = 0;
    int count = 1;
    int aux = 0;
    std::string enchantments;
    std::string name;
    std::string lore1;
    std::string lore2;
    std::string lore3;
    std::string lore4;
    std::string lore5;

    constexpr const char* containerTypeToString(SlotType v) {
        switch (v) {
            case SlotType::Mainhand:    return "Mainhand";
            case SlotType::Offhand:     return "Offhand";
            case SlotType::Armor:       return "Armor";
            case SlotType::Inventory:   return "Inventory";
            case SlotType::Enderchest:  return "Enderchest";
            default:                    return "Unknown";
        }
    }

    void giveItem(Player *player, CommandOutput &output, CommandItem &cmi, ItemStack &is,
        bool hasEnchantments, bool hasName, bool hasLore, bool sendCommandFeedback) {

        int maxStackSize = is.getMaxStackSize();
        if (count > maxStackSize) {
            count = maxStackSize;
        }
        cmi.createInstance(&is, count, aux, output, false);

        if (hasEnchantments) {
            EnchantmentInstance instance;
            std::vector<Enchantment> enchantmentsVector;
            getEnchantmentsFromString(enchantments.c_str(), enchantmentsVector);
            fromNbtCommand = true;
            for (auto& enchant : enchantmentsVector) {
                instance.type = (Enchant::Type) enchant.id;
                instance.level = enchant.level;
                EnchantUtils::applyEnchant(is, instance, true);
            }
            fromNbtCommand = false;
        }

        if (hasName) {
            is.setCustomName(name);
        }

        if (hasLore) {
            std::vector<std::string> loreVector = { lore1, lore2, lore3, lore4, lore5 };
            while (loreVector.size() > 0 && loreVector.back().empty()) {
                loreVector.pop_back();
            }
            is.setCustomLore(loreVector);
        }

        switch (type) {
            case SlotType::Mainhand:
                player->setCarriedItem(ItemStack::EMPTY_ITEM);
                player->setCarriedItem(is);
                break;

            case SlotType::Offhand:
                player->setOffhandSlot(ItemStack::EMPTY_ITEM);
                player->setOffhandSlot(is);
                break;

            case SlotType::Armor:
                player->setArmor((ArmorSlot) slotId, ItemStack::EMPTY_ITEM);
                player->setArmor((ArmorSlot) slotId, is);
                break;

            case SlotType::Inventory: // hotbar and inventory - slot ids 0-8 and 9-35 respectively
                {     
                    Inventory* playerInventory = CallServerClassMethod<PlayerInventory*>(
                        "?getSupplies@Player@@QEAAAEAVPlayerInventory@@XZ", player)->inventory.get();
                    playerInventory->setItem(slotId, ItemStack::EMPTY_ITEM);
                    playerInventory->setItem(slotId, is);
                    break;
                }

            case SlotType::Enderchest:
                {     
                    EnderChestContainer* playerEnderChest = player->getEnderChestContainer();
                    playerEnderChest->setItem(slotId, ItemStack::EMPTY_ITEM);
                    playerEnderChest->setItem(slotId, is);
                    break;
                }
            
            default: break;
        }

        CallServerClassMethod<void>("?sendInventory@ServerPlayer@@UEAAX_N@Z", player, false);

        if (sendCommandFeedback) {
            auto receivedItemMessage = TextPacket::createTextPacket<TextPacketType::SystemMessage>(
                "§a[ReplaceItemNBT]§r You have been set with * " + std::to_string(count) + " of item with ID " + std::to_string(id) + ":" + std::to_string(aux) + " in slot " + containerTypeToString(type) + ":" + std::to_string(slotId));
            player->sendNetworkPacket(receivedItemMessage);
        }
    }

    void execute(CommandOrigin const &origin, CommandOutput &output) {

        std::string stringifiedSlotId = std::to_string(slotId);
        std::string stringifiedCount = std::to_string(count);
        std::string stringifiedAux = std::to_string(aux);
        switch (type) {
            case SlotType::Mainhand ... SlotType::Offhand:
                if (slotId != 0) {
                    return output.error("The slot ID you have entered (" + stringifiedSlotId + ") must be exactly 0");
                }

            case SlotType::Armor:
                if (slotId < 0 || slotId > 3) {
                    return output.error("The slot ID you have entered (" + stringifiedSlotId + ") is not within the allowed range of 0-3");
                }
                
            case SlotType::Inventory:
                if (slotId < 0 || slotId > 35) {
                    return output.error("The slot ID you have entered (" + stringifiedSlotId + ") is not within the allowed range of 0-35");
                }

            case SlotType::Enderchest:
                if (slotId < 0 || slotId > 26) {
                    return output.error("The slot ID you have entered (" + stringifiedSlotId + ") is not within the allowed range of 0-26");
                }

            default: break;
        }
        if (count < 1 || count > 64) {
            return output.error("The count you have entered (" + stringifiedCount + ") is not within the allowed range of 1-64");
        }
        else if (aux < 0 || aux > 32767) {
            return output.error("The aux value you have entered (" + stringifiedAux + ") is not within the allowed range of 0-32767");
        }

        auto selectedEntities = selector.results(origin);
        if (selectedEntities.empty()) {
            return output.error("No targets matched selector");
        }

        CommandItem cmi;
        cmi.id = id;
        ItemStack is;
        cmi.createInstance(&is, 0, aux, output, false);
        if (is.isNull()) {
            return output.error("Specified item ID is invalid");
        }

        bool hasEnchantments = checkEnchantmentString(enchantments, output);
        bool hasName = !name.empty();
        std::string lore[5] = { lore1, lore2, lore3, lore4, lore5 };
        bool hasLore = !std::all_of(lore, std::end(lore), LIFT(std::empty));
        bool sendCommandFeedback = getBoolFromGameruleId(17);

        for (auto player : selectedEntities) {
            giveItem(player, output, cmi, is, hasEnchantments, hasName, hasLore, sendCommandFeedback);
        }
        output.success(
            "§a[ReplaceItemNBT]§r Set * " + stringifiedCount + " of item with ID " + std::to_string(id) + ":" + stringifiedAux + " in slot " + containerTypeToString(type) + ":" + stringifiedSlotId + " for " + std::to_string(selectedEntities.count()) + (selectedEntities.count() == 1 ? " player" : " players"));
    }

    static void setup(CommandRegistry *registry) {
        using namespace commands;
        registry->registerCommand(
            "replaceitemnbt", "sets an item with custom NBT in a specified equipment slot.", CommandPermissionLevel::GameMasters, CommandFlagCheat, CommandFlagNone);
        
        commands::addEnum<SlotType>(registry, "slotType", {
            { "slot.weapon.mainhand", SlotType::Mainhand },
            { "slot.weapon.offhand", SlotType::Offhand },
            { "slot.armor", SlotType::Armor },
            { "slot.inventory", SlotType::Inventory },
            { "slot.enderchest", SlotType::Enderchest },
        });

        registry->registerOverload<ReplaceItemNbtCommand>("replaceitemnbt",
            mandatory(&ReplaceItemNbtCommand::selector, "target"),
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

TClasslessInstanceHook(void, "?load@FunctionManager@@QEAAXAEAVResourcePackManager@@AEAVCommandRegistry@@@Z",
    class ResourcePackManager &rpManager, class CommandRegistry &registry) {
    GiveNbtCommand::setup(&registry);
    ReplaceItemNbtCommand::setup(&registry);
    original(this, rpManager, registry);
}
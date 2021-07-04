#include "global.h"
#include <dllentry.h>

#define FWD(...) \
  std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)

#define LIFT(...) \
  [](auto&&... args) \
    noexcept(noexcept(__VA_ARGS__(FWD(args)...)))  \
    -> decltype(__VA_ARGS__(FWD(args)...)) \
  { return __VA_ARGS__(FWD(args)...); }

void PreInit() {
  Mod::CommandSupport::GetInstance().AddListener(SIG("loaded"), initCommand);
}
void PostInit() {}
void dllenter() {}
void dllexit() {}

bool fromGiveNbtCommand = false;

THook(EnchantResult*, "?canEnchant@ItemEnchants@@QEAA?AUEnchantResult@@VEnchantmentInstance@@_N@Z",
  ItemEnchants *self, EnchantmentInstance enchant, bool allowNonVanilla) {
    auto ret = original(self, enchant, allowNonVanilla);
    if (fromGiveNbtCommand) {
      //force enchant
      if(ret->result != EnchantResultType::Enchant) {
        ret->result = EnchantResultType::Enchant;
      }
    }
    return ret;
}
static_assert(sizeof(EnchantResult) == 24);

class GiveNbtCommand : public Command {
public:
  GiveNbtCommand() { selector.setIncludeDeadPlayers(false); }

  //command arguments, also initialize values
  CommandSelector<Player> selector;
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

  class CommandItem {
    public:

      //necessary initializations
      int version = 0;
      int id = 0;
      ItemStackBase *createInstance(ItemStackBase *is, int count, int aux, CommandOutput &output, bool exactAux) {
        return CallServerClassMethod<ItemStackBase *>(
          "?createInstance@CommandItem@@QEBA?AV?$optional@VItemInstance@@@std@@HHPEAVCommandOutput@@_N@Z", this, is,
          count, aux, &output, exactAux);
    }
  };

  bool dropItem(Player *player, ItemStack const &is, bool random) {
    return CallServerClassMethod<bool>("?drop@Player@@UEAA_NAEBVItemStack@@_N@Z", player, &is, false);
  }

  bool addItem(Player *player, ItemStack &is) {
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

  struct Enchantment {
    short id;
    short level;
    Enchantment(short id, short level) : id(id), level(level) {}
    Enchantment() : id(0), level(0) {}
  };
  
  void getEnchantmentsFromString(const char* string, std::vector<Enchantment>& out_split) {
    while (string[0] != 0) { //while its not the end of the string
      Enchantment newEnchantment;
      newEnchantment.id = std::clamp(std::atoi(string), 0, 35); //convert to ascii to number, clamp to vanilla enchantment ids
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
        while (string[0] != 0 && string[0] != ',') {//continue parsing!
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

  void giveItem(Player *player, CommandOutput &output, CommandItem &cmi, ItemStack &is) {

    bool hasEnchantments = checkEnchantmentString(enchantments, output);
    bool hasName = !name.empty();
    //bool hasLore = !lore1.empty() || !lore2.empty() || !lore3.empty(); || !lore4.empty(); || !lore5.empty();
    std::string lore[5] = { lore1, lore2, lore3, lore4, lore5 };
    bool hasLore = !std::all_of(lore, std::end(lore), LIFT(std::empty));

    //loop through count to give more than 1 stack of the item
    const int playerInventorySlots = 36;
    int maxStackSize = is.getMaxStackSize();
    int countNew = count;
    countNew = std::min(countNew, maxStackSize * playerInventorySlots);
    
    while (countNew > 0) {
      int this_stack = std::min(maxStackSize, countNew);
      cmi.createInstance(&is, this_stack, aux, output, false);
      countNew -= this_stack;

      //apply NBT to created item
      if (hasEnchantments) {
        EnchantmentInstance instance;
        std::vector<Enchantment> enchantmentsVector;
        getEnchantmentsFromString(enchantments.c_str(), enchantmentsVector);
        fromGiveNbtCommand = true;

        //loop through results of getEnchantmentsFromString and apply to item instance
        for (auto& enchant : enchantmentsVector) {
          //std::cout << "ID: " << enchant.id << "   Level: " << enchant.level << std::endl;
          instance.type = (Enchant::Type) enchant.id;//type of enchant
          instance.level = enchant.level;//tier of enchant
          EnchantUtils::applyEnchant(is, instance, true);
        }
      }
      fromGiveNbtCommand = false;

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

      //add item to player inventory
      //if item cannot be added to player inventory, drop item
      if (!addItem(player, is)) {
        dropItem(player, is, false);
      }
    }

    //update inventory to the client
    CallServerClassMethod<void>("?sendInventory@ServerPlayer@@UEAAX_N@Z", player, false);
    output.success("§a[GiveNBT]§r You have been given * " + std::to_string(count) + " of item with ID " + std::to_string(id) + ":" + std::to_string(aux));
  }

  void execute(CommandOrigin const &origin, CommandOutput &output) {
    if (count <= 0) {
      output.error("The count you have entered (" + std::to_string(count) + ") is too small, it must be at least 1");
      return;
    }
    else if (count > 32767) {
      output.error("The count you have entered (" + std::to_string(count) + ") is too big, it must be at most 32767");
      return;
    }
    else if (aux < 0) {
      output.error("The aux value you have entered (" + std::to_string(aux) + ") is too small, it must be at least 0");
      return;
    }
    else if (aux > 32767) {
      output.error("The aux value you have entered (" + std::to_string(aux) + ") is too big, it must be at most 32767");
      return;
    }

    //get selected entity/entities
    auto selectedEntities = selector.results(origin);
    if (selectedEntities.empty()) {
      //output.error("commands.generic.noTargetMatch");
      output.error("No targets matched selector");
      return;
    }

    //hack: create empty instance of an item to test ID and stack size
    //using id and aux from in-game command parameters
    CommandItem cmi;
    cmi.id = id;
    ItemStack is;
    cmi.createInstance(&is, 0, aux, output, false);
    if (is.isNull()) {
      output.error("Specified item ID is invalid");
      return;
    }

    //loop through selector results
    //pass in CommandItem and ItemStack from empty instance
    for (auto player : selectedEntities) {
      giveItem(player, output, cmi, is);
    }
  }
  static void setup(CommandRegistry *registry) {
    using namespace commands;
    registry->registerCommand(
      "givenbt", "Gives an item with custom NBT.", CommandPermissionLevel::GameMasters, CommandFlagCheat, CommandFlagNone);
    registry->registerOverload<GiveNbtCommand>("givenbt",
      mandatory(&GiveNbtCommand::selector, "target"),
      mandatory(&GiveNbtCommand::id, "item id"),
      optional(&GiveNbtCommand::count, "count"),
      optional(&GiveNbtCommand::aux, "aux value"),
      optional(&GiveNbtCommand::enchantments, "enchantments"),
      optional(&GiveNbtCommand::name, "custom name"),
      optional(&GiveNbtCommand::lore1, "lore line 1"),
      optional(&GiveNbtCommand::lore2, "lore line 2"),
      optional(&GiveNbtCommand::lore3, "lore line 3"),
      optional(&GiveNbtCommand::lore4, "lore line 4"),
      optional(&GiveNbtCommand::lore5, "lore line 5")
    );
  }
};

void initCommand(CommandRegistry *registry) {
  GiveNbtCommand::setup(registry);
}
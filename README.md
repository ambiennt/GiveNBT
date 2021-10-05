# givenbt
## give and replace items with custom NBT on BDS + ElementZero 1.16.20-.40

### givenbt syntax:
#### /givenbt <target: target> <item id: int> [count: int] [aux value: int] [enchantments: string] [custom name: string] [lore line 1: string] [lore line 2: string] [lore line 3: string] [lore line 4: string] [lore line 5: string]

The 2 necessary parameters to this command are the target selector (players only) and the item ID. Even without adding NBT, this command may still be useful to give illegal items to players, because the command checks for the numerical item ID instead of its string name.

an example of the command:
/givenbt @a 247 100 0 "9:20,18:5" "my netherreactor core" "§6This is a special item from minecraft pocket edition!" "§c*note:" "§7this is just a decorative block"

output:
![image](https://user-images.githubusercontent.com/63216972/124373843-bbb7af00-dc4a-11eb-9139-09d55c2f0303.png)
  
<br/><br/><br/>
  
### replaceitemnbt syntax:
#### /replaceitemnbt <target: target> \<slot type: int\> \<slot id: int\> \<item id: int\> [count: int] [aux value: int] [enchantments: string] [custom name: string] [lore line 1: string] [lore line 2: string] [lore line 3: string] [lore line 4: string] [lore line 5: string]

The 2 necessary parameters to this command are the target selector (players only), the slot type, slot ID, and item ID. The available slot types and their corresponding IDs are as follows:

  Slot type 0 = mainhand (slot ID 0 only)<br/>
  Slot type 1 = offhand (slot ID 0 only)<br/>
  Slot type 2 = armor (slot ID 0-3)<br/>
  Slot type 3 = inventory (slot ID 0-8 for hotbar, 9-35 for inventory)<br/>
  
<br/><br/><br/>
  
Something important to note about the enchantment string parameter is the delimiters. Use the : to separate enchantment ID from level, and , to separate individual enchantments.

List of bedrock edition item IDs:
https://minecraft.fandom.com/wiki/Bedrock_Edition_data_values

If you wish to give yourself a custom item, the ID will start at 802 (next number after the highest item ID, soul campfire, which is 801). Mutliple custom items will add to the item ID registry in alphabetical order.

List of bedrock edition enchantment IDs:
https://www.digminecraft.com/lists/enchantment_list_pe.php

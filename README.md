# givenbt
## give and replace items with custom NBT on BDS + ElementZero 1.16.20-.40

### givenbt syntax:
#### /givenbt \<target: target\> \<itemId: int\> [count: int] [auxValue: int] [toEnderChest: boolean] [enchantments: string] [customName: string] [lore1: string] [lore2: string] [lore3: string] [lore4: string] [lore5: string]

The 2 necessary parameters to this command are the target selector (players only) and the item ID. Even without adding NBT, this command may still be useful to give illegal items to players, because the command checks for the numerical item ID instead of its string name. the toEnderChest parameter dictates whether the item(s) will be added to the target's inventory or ender chest.

an example of the command:
/givenbt @a 247 100 0 "9:20,18:5" "my netherreactor core" "§6This is a special item from minecraft pocket edition!" "§c*note:" "§7this is just a decorative block"

output:
![image](https://user-images.githubusercontent.com/63216972/124373843-bbb7af00-dc4a-11eb-9139-09d55c2f0303.png)
  
<br/><br/><br/>
  
### replaceitemnbt syntax:
#### /replaceitemnbt \<target: target\> \<slotType: type\> \<slotId: int\> \<itemId: int\> [count: int] [auxValue: int] [enchantments: string] [customName: string] [lore1: string] [lore2: string] [lore3: string] [lore4: string] [lore5: string]

The 2 necessary parameters to this command are the target selector (players only), the slot type, slot ID, and item ID.
  
<br/><br/><br/>
  
Something important to note about the enchantment string parameter is the delimiters. Use the : to separate enchantment ID from level, and , to separate individual enchantments.

List of bedrock edition item IDs:
https://minecraft.fandom.com/wiki/Bedrock_Edition_data_values

If you wish to give yourself a custom item, the ID will start at 802 (next number after the highest item ID, soul campfire, which is 801). Mutliple custom items will add to the item ID registry in alphabetical order.

List of bedrock edition enchantment IDs:
https://www.digminecraft.com/lists/enchantment_list_pe.php

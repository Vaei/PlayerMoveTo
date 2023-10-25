# PlayerMoveTo
Adds nodes equivalent to AIMoveTo for PlayerController

The goal is to provide pathfinding for PlayerController, for use-cases like making the player character run to the car's door in GTA, or towards a lever before pulling it, or auto-move (eg. horse following road in some games)

`GP Move To Actor or Location` is available for gameplay abilities (only) within Unreal Engine, but requires additional components, and didn't modify the acceleration so animations don't play.

There is an additional node added to this plugin for use outside of gameplay abilities as well.

These nodes will be available with the use of this plugin:
* `Gameplay Player Move To Actor or Location`
* `Player Move To Actor or Location`

This plugin was created using Unreal 5.1

## How to Use
Clone to your project's "Plugins" folder (you may need to create this).

Then simply add the nodes described above as required.

If using the `Player Move To Actor or Location` then you will need to inherit your PlayerController from `APlayerAIMoveToController`. If you cannot inherit for any reason, then copy/paste the logic into your own PlayerController.

## Example Usage
![example usage](https://github.com/Vaei/repo_files/blob/main/PlayerMoveTo/gameplay_playermoveto.png)
![example usage](https://github.com/Vaei/repo_files/blob/main/PlayerMoveTo/controller_playermoveto.png)

## Versions
### 1.0.1.0
* Added null check causing crash on sim proxies

### 1.0.0.0
* Initial Release

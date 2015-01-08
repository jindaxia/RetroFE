/* This file is part of RetroFE.
 *
 * RetroFE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * RetroFE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with RetroFE.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Item.h"
#include "../Utility/Utils.h"
#include <sstream>
#include <algorithm>

Item::Item()
    : NumberPlayers(0)
    , NumberButtons(0)
    , Leaf(true)
{
}

Item::~Item()
{
}

const std::string Item::GetFileName() const
{
    return Utils::GetFileName(FilePath);
}

const std::string& Item::GetFilePath() const
{
    return FilePath;
}

void Item::SetFilePath(const std::string& filepath)
{
    FilePath = filepath;
}

const std::string& Item::GetLauncher() const
{
    return Launcher;
}

void Item::SetLauncher(const std::string& launcher)
{
    Launcher = launcher;
}

const std::string& Item::GetManufacturer() const
{
    return Manufacturer;
}

void Item::SetManufacturer(const std::string& manufacturer)
{
    Manufacturer = manufacturer;
}

const std::string& Item::GetName() const
{
    return Name;
}

void Item::SetName(const std::string& name)
{
    Name = name;
}

int Item::GetNumberButtons() const
{
    return NumberButtons;
}

std::string Item::GetNumberButtonsString()
{
    std::stringstream ss;
    ss << NumberButtons;
    return ss.str();
}


void Item::SetNumberButtons(int numberbuttons)
{
    NumberButtons = numberbuttons;
}

int Item::GetNumberPlayers() const
{
    return NumberPlayers;
}

std::string Item::GetNumberPlayersString()
{
    std::stringstream ss;
    ss << NumberButtons;
    return ss.str();
}


void Item::SetNumberPlayers(int numberplayers)
{
    NumberPlayers = numberplayers;
}

const std::string& Item::GetTitle() const
{
    return Title;
}

const std::string& Item::GetLCTitle() const
{
    return LCTitle;
}

void Item::SetTitle(const std::string& title)
{
    Title = title;
    LCTitle = Title;
    std::transform(LCTitle.begin(), LCTitle.end(), LCTitle.begin(), ::tolower);
}

const std::string& Item::GetYear() const
{
    return Year;
}

void Item::SetYear(const std::string& year)
{
    Year = year;
}

bool Item::IsLeaf() const
{
    return Leaf;
}

void Item::SetIsLeaf(bool leaf)
{
    Leaf = leaf;
}

const std::string& Item::GetFullTitle() const
{
    return FullTitle;
}

void Item::SetFullTitle(const std::string& fulltitle)
{
    FullTitle = fulltitle;
}

const std::string& Item::GetCloneOf() const
{
    return CloneOf;
}

void Item::SetCloneOf(const std::string& cloneOf)
{
    CloneOf = cloneOf;
}

bool Item::operator<(const Item &rhs)
{
    return LCTitle < rhs.LCTitle;
}
bool Item::operator>(const Item &rhs)
{
    return LCTitle > rhs.LCTitle;
}

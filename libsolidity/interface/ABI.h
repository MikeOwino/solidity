/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0
/**
 * Utilities to handle the Contract ABI (https://docs.soliditylang.org/en/develop/abi-spec.html)
 */

#pragma once

#include <libsolutil/JSON.h>
#include <memory>
#include <string>

namespace solidity::frontend
{

// Forward declarations
class ContractDefinition;
class Type;

class ABI
{
public:
	/// Get the ABI Interface of the contract
	/// @param _contractDef The contract definition
	/// @return             A JSON representation of the contract's ABI Interface
	static Json generate(ContractDefinition const& _contractDef);
private:
	/// @returns a json value suitable for a list of types in function input or output
	/// parameters or other places. If @a _forLibrary is true, complex types are referenced
	/// by name, otherwise they are anonymously expanded.
	/// @a _solidityTypes is the list of original Solidity types where @a _encodingTypes is the list of
	/// ABI types used for the actual encoding.
	static Json formatTypeList(
		std::vector<std::string> const& _names,
		std::vector<Type const*> const& _encodingTypes,
		std::vector<Type const*> const& _solidityTypes,
		bool _forLibrary
	);
	/// @returns a Json object with "name", "type", "internalType" and potentially
	/// "components" keys, according to the ABI specification.
	/// If it is possible to express the type as a single string, it is allowed to return a single string.
	/// @a _solidityType is the original Solidity type and @a _encodingTypes is the
	/// ABI type used for the actual encoding.
	static Json formatType(
		std::string const& _name,
		Type const& _encodingType,
		Type const& _solidityType,
		bool _forLibrary
	);
};

}

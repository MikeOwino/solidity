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
 * @file KnownState.cpp
 * @author Christian <c@ethdev.com>
 * @date 2015
 * Contains knowledge about the state of the virtual machine at a specific instruction.
 */

#include <libevmasm/KnownState.h>
#include <libevmasm/AssemblyItem.h>
#include <libsolutil/Keccak256.h>

#include <functional>
#include <utility>

using namespace solidity;
using namespace solidity::evmasm;
using namespace solidity::langutil;

std::ostream& KnownState::stream(std::ostream& _out) const
{
	auto streamExpressionClass = [this](std::ostream& _out, Id _id)
	{
		auto const& expr = m_expressionClasses->representative(_id);
		_out << "  " << std::dec << _id << ": ";
		if (!expr.item)
			_out << " no item";
		else if (expr.item->type() == UndefinedItem)
			_out << " unknown " << static_cast<int>(expr.item->data());
		else
			_out << *expr.item;
		if (expr.sequenceNumber)
			_out << "@" << std::dec << expr.sequenceNumber;
		_out << "(";
		for (Id arg: expr.arguments)
			_out << std::dec << arg << ",";
		_out << ")" << std::endl;
	};

	_out << "=== State ===" << std::endl;
	_out << "Stack height: " << std::dec << m_stackHeight << std::endl;
	_out << "Equivalence classes:" << std::endl;
	for (Id eqClass = 0; eqClass < m_expressionClasses->size(); ++eqClass)
		streamExpressionClass(_out, eqClass);

	_out << "Stack:" << std::endl;
	for (auto const& it: m_stackElements)
	{
		_out << "  " << std::dec << it.first << ": ";
		streamExpressionClass(_out, it.second);
	}
	_out << "Storage:" << std::endl;
	for (auto const& it: m_storageContent)
	{
		_out << "  ";
		streamExpressionClass(_out, it.first);
		_out << ": ";
		streamExpressionClass(_out, it.second);
	}
	_out << "Memory:" << std::endl;
	for (auto const& it: m_memoryContent)
	{
		_out << "  ";
		streamExpressionClass(_out, it.first);
		_out << ": ";
		streamExpressionClass(_out, it.second);
	}

	return _out;
}

KnownState::StoreOperation KnownState::feedItem(AssemblyItem const& _item, bool _copyItem)
{
	StoreOperation op;
	if (_item.type() == Tag)
	{
		// can be ignored
	}
	else if (_item.type() == AssignImmutable)
	{
		// Since AssignImmutable breaks blocks, it should be fine to only consider its changes to the stack, which
		// is the same as two POPs.
		// Note that the StoreOperation for POP is generic and _copyItem is ignored.
		feedItem(AssemblyItem(Instruction::POP), _copyItem);
		return feedItem(AssemblyItem(Instruction::POP), _copyItem);
	}
	else if (_item.type() == VerbatimBytecode)
	{
		m_sequenceNumber += 2;
		resetMemory();
		resetKnownKeccak256Hashes();
		resetStorage();
		// Consume all arguments and place unknown return values on the stack.
		m_stackElements.erase(
			m_stackElements.upper_bound(m_stackHeight - static_cast<int>(_item.arguments())),
			m_stackElements.end()
		);
		m_stackHeight += static_cast<int>(_item.deposit());
		for (size_t i = 0; i < _item.returnValues(); ++i)
			setStackElement(
				m_stackHeight - static_cast<int>(i),
				m_expressionClasses->newClass(_item.debugData())
			);
	}
	else if (_item.type() != Operation)
	{
		solAssert(_item.deposit() == 1);
		if (_item.pushedValue())
			// only available after assembly stage, should not be used for optimisation
			setStackElement(++m_stackHeight, m_expressionClasses->find(*_item.pushedValue()));
		else
			setStackElement(++m_stackHeight, m_expressionClasses->find(_item, {}, _copyItem));
	}
	else
	{
		Instruction instruction = _item.instruction();
		// The latest EVMVersion is used here, since the InstructionInfo is assumed to be
		// the same across all EVM versions except for the instruction name.
		InstructionInfo info = instructionInfo(instruction, EVMVersion());
		if (SemanticInformation::isDupInstruction(_item))
			setStackElement(
				m_stackHeight + 1,
				stackElement(
					m_stackHeight - (static_cast<int>(SemanticInformation::getDupNumber(_item)) - 1),
					_item.debugData()
				)
			);
		else if (SemanticInformation::isSwapInstruction(_item))
			swapStackElements(
				m_stackHeight,
				m_stackHeight - 1 - (static_cast<int>(SemanticInformation::getSwapNumber(_item)) - 1),
				_item.debugData()
			);
		else if (instruction != Instruction::POP)
		{
			std::vector<Id> arguments(static_cast<size_t>(info.args));
			for (size_t i = 0; i < static_cast<size_t>(info.args); ++i)
				arguments[i] = stackElement(m_stackHeight - static_cast<int>(i), _item.debugData());
			switch (_item.instruction())
			{
			case Instruction::SSTORE:
				op = storeInStorage(arguments[0], arguments[1], _item.debugData());
				break;
			case Instruction::SLOAD:
				setStackElement(
					m_stackHeight + static_cast<int>(_item.deposit()),
					loadFromStorage(arguments[0], _item.debugData())
				);
				break;
			case Instruction::MSTORE:
				op = storeInMemory(arguments[0], arguments[1], _item.debugData());
				break;
			case Instruction::MLOAD:
				setStackElement(
					m_stackHeight + static_cast<int>(_item.deposit()),
					loadFromMemory(arguments[0], _item.debugData())
				);
				break;
			case Instruction::KECCAK256:
				setStackElement(
					m_stackHeight + static_cast<int>(_item.deposit()),
					applyKeccak256(arguments.at(0), arguments.at(1), _item.debugData())
				);
				break;
			default:
				bool invMem =
					SemanticInformation::memory(_item.instruction()) == SemanticInformation::Write;
				bool invStor =
					SemanticInformation::storage(_item.instruction()) == SemanticInformation::Write;
				// We could be a bit more fine-grained here (CALL only invalidates part of
				// memory, etc), but we do not for now.
				if (invMem)
				{
					resetMemory();
					resetKnownKeccak256Hashes();
				}
				if (invStor)
					resetStorage();
				if (invMem || invStor)
					m_sequenceNumber += 2; // Increment by two because it can read and write
				solAssert(info.ret <= 1);
				if (info.ret == 1)
					setStackElement(
						m_stackHeight + static_cast<int>(_item.deposit()),
						m_expressionClasses->find(_item, arguments, _copyItem)
					);
			}
		}
		m_stackElements.erase(
			m_stackElements.upper_bound(m_stackHeight + static_cast<int>(_item.deposit())),
			m_stackElements.end()
		);
		m_stackHeight += static_cast<int>(_item.deposit());
	}
	return op;
}

/// Helper function for KnownState::reduceToCommonKnowledge, removes everything from
/// _this which is not in or not equal to the value in _other.
template <class Mapping> void intersect(Mapping& _this, Mapping const& _other)
{
	for (auto it = _this.begin(); it != _this.end();)
		if (_other.count(it->first) && _other.at(it->first) == it->second)
			++it;
		else
			it = _this.erase(it);
}

void KnownState::reduceToCommonKnowledge(KnownState const& _other, bool _combineSequenceNumbers)
{
	int stackDiff = m_stackHeight - _other.m_stackHeight;
	for (auto it = m_stackElements.begin(); it != m_stackElements.end();)
		if (_other.m_stackElements.count(it->first - stackDiff))
		{
			Id other = _other.m_stackElements.at(it->first - stackDiff);
			if (it->second == other)
				++it;
			else
			{
				std::set<u256> theseTags = tagsInExpression(it->second);
				std::set<u256> otherTags = tagsInExpression(other);
				if (!theseTags.empty() && !otherTags.empty())
				{
					theseTags.insert(otherTags.begin(), otherTags.end());
					it->second = tagUnion(theseTags);
					++it;
				}
				else
					it = m_stackElements.erase(it);
			}
		}
		else
			it = m_stackElements.erase(it);

	// Use the smaller stack height. Essential to terminate in case of loops.
	if (m_stackHeight > _other.m_stackHeight)
	{
		std::map<int, Id> shiftedStack;
		for (auto const& stackElement: m_stackElements)
			shiftedStack[stackElement.first - stackDiff] = stackElement.second;
		m_stackElements = std::move(shiftedStack);
		m_stackHeight = _other.m_stackHeight;
	}

	intersect(m_storageContent, _other.m_storageContent);
	intersect(m_memoryContent, _other.m_memoryContent);
	if (_combineSequenceNumbers)
		m_sequenceNumber = std::max(m_sequenceNumber, _other.m_sequenceNumber);
}

bool KnownState::operator==(KnownState const& _other) const
{
	if (m_storageContent != _other.m_storageContent || m_memoryContent != _other.m_memoryContent)
		return false;
	int stackDiff = m_stackHeight - _other.m_stackHeight;
	auto thisIt = m_stackElements.cbegin();
	auto otherIt = _other.m_stackElements.cbegin();
	for (; thisIt != m_stackElements.cend() && otherIt != _other.m_stackElements.cend(); ++thisIt, ++otherIt)
		if (thisIt->first - stackDiff != otherIt->first || thisIt->second != otherIt->second)
			return false;
	return (thisIt == m_stackElements.cend() && otherIt == _other.m_stackElements.cend());
}

ExpressionClasses::Id KnownState::stackElement(int _stackHeight, langutil::DebugData::ConstPtr _debugData)
{
	if (m_stackElements.count(_stackHeight))
		return m_stackElements.at(_stackHeight);
	// Stack element not found (not assigned yet), create new unknown equivalence class.
	return m_stackElements[_stackHeight] =
			m_expressionClasses->find(AssemblyItem(UndefinedItem, _stackHeight, std::move(_debugData)));
}

KnownState::Id KnownState::relativeStackElement(int _stackOffset, langutil::DebugData::ConstPtr _debugData)
{
	return stackElement(m_stackHeight + _stackOffset, std::move(_debugData));
}

void KnownState::clearTagUnions()
{
	for (auto it = m_stackElements.begin(); it != m_stackElements.end();)
		if (m_tagUnions.left.count(it->second))
			it = m_stackElements.erase(it);
		else
			++it;
}

void KnownState::setStackElement(int _stackHeight, Id _class)
{
	m_stackElements[_stackHeight] = _class;
}

void KnownState::swapStackElements(
	int _stackHeightA,
	int _stackHeightB,
	langutil::DebugData::ConstPtr _debugData
)
{
	assertThrow(_stackHeightA != _stackHeightB, OptimizerException, "Swap on same stack elements.");
	// ensure they are created
	stackElement(_stackHeightA, _debugData);
	stackElement(_stackHeightB, _debugData);

	std::swap(m_stackElements[_stackHeightA], m_stackElements[_stackHeightB]);
}

KnownState::StoreOperation KnownState::storeInStorage(
	Id _slot,
	Id _value,
	langutil::DebugData::ConstPtr _debugData
)
{
	if (m_storageContent.count(_slot) && m_storageContent[_slot] == _value)
		// do not execute the storage if we know that the value is already there
		return StoreOperation();
	m_sequenceNumber++;
	decltype(m_storageContent) storageContents;
	// Copy over all values (i.e. retain knowledge about them) where we know that this store
	// operation will not destroy the knowledge. Specifically, we copy storage locations we know
	// are different from _slot or locations where we know that the stored value is equal to _value.
	for (auto const& storageItem: m_storageContent)
		if (m_expressionClasses->knownToBeDifferent(storageItem.first, _slot) || storageItem.second == _value)
			storageContents.insert(storageItem);
	m_storageContent = std::move(storageContents);

	AssemblyItem item(Instruction::SSTORE, std::move(_debugData));
	Id id = m_expressionClasses->find(item, {_slot, _value}, true, m_sequenceNumber);
	StoreOperation operation{StoreOperation::Storage, _slot, m_sequenceNumber, id};
	m_storageContent[_slot] = _value;
	// increment a second time so that we get unique sequence numbers for writes
	m_sequenceNumber++;

	return operation;
}

ExpressionClasses::Id KnownState::loadFromStorage(Id _slot, langutil::DebugData::ConstPtr _debugData)
{
	if (m_storageContent.count(_slot))
		return m_storageContent.at(_slot);

	AssemblyItem item(Instruction::SLOAD, std::move(_debugData));
	return m_storageContent[_slot] = m_expressionClasses->find(item, {_slot}, true, m_sequenceNumber);
}

KnownState::StoreOperation KnownState::storeInMemory(Id _slot, Id _value, langutil::DebugData::ConstPtr _debugData)
{
	if (m_memoryContent.count(_slot) && m_memoryContent[_slot] == _value)
		// do not execute the store if we know that the value is already there
		return StoreOperation();
	m_sequenceNumber++;
	decltype(m_memoryContent) memoryContents;
	// copy over values at points where we know that they are different from _slot by at least 32
	for (auto const& memoryItem: m_memoryContent)
		if (m_expressionClasses->knownToBeDifferentBy32(memoryItem.first, _slot))
			memoryContents.insert(memoryItem);
	m_memoryContent = std::move(memoryContents);

	AssemblyItem item(Instruction::MSTORE, std::move(_debugData));
	Id id = m_expressionClasses->find(item, {_slot, _value}, true, m_sequenceNumber);
	StoreOperation operation{StoreOperation::Memory, _slot, m_sequenceNumber, id};
	m_memoryContent[_slot] = _value;
	// increment a second time so that we get unique sequence numbers for writes
	m_sequenceNumber++;
	return operation;
}

ExpressionClasses::Id KnownState::loadFromMemory(Id _slot, langutil::DebugData::ConstPtr _debugData)
{
	if (m_memoryContent.count(_slot))
		return m_memoryContent.at(_slot);

	AssemblyItem item(Instruction::MLOAD, std::move(_debugData));
	return m_memoryContent[_slot] = m_expressionClasses->find(item, {_slot}, true, m_sequenceNumber);
}

KnownState::Id KnownState::applyKeccak256(
	Id _start,
	Id _length,
	langutil::DebugData::ConstPtr _debugData
)
{
	AssemblyItem keccak256Item(Instruction::KECCAK256, _debugData);
	// Special logic if length is a short constant, otherwise we cannot tell.
	u256 const* l = m_expressionClasses->knownConstant(_length);
	// unknown or too large length
	if (!l || *l > 128)
		return m_expressionClasses->find(keccak256Item, {_start, _length}, true, m_sequenceNumber);
	unsigned length = unsigned(*l);
	std::vector<Id> arguments;
	for (unsigned i = 0; i < length; i += 32)
	{
		Id slot = m_expressionClasses->find(
			AssemblyItem(Instruction::ADD, _debugData),
			{_start, m_expressionClasses->find(u256(i))}
		);
		arguments.push_back(loadFromMemory(slot, _debugData));
	}
	if (m_knownKeccak256Hashes.count({arguments, length}))
		return m_knownKeccak256Hashes.at({arguments, length});
	Id v;
	// If all arguments are known constants, compute the Keccak-256 here
	if (all_of(arguments.begin(), arguments.end(), [this](Id _a) { return !!m_expressionClasses->knownConstant(_a); }))
	{
		bytes data;
		for (Id a: arguments)
			data += toBigEndian(*m_expressionClasses->knownConstant(a));
		data.resize(length);
		v = m_expressionClasses->find(AssemblyItem(u256(util::keccak256(data)), _debugData));
	}
	else
		v = m_expressionClasses->find(keccak256Item, {_start, _length}, true, m_sequenceNumber);
	return m_knownKeccak256Hashes[{arguments, length}] = v;
}

std::set<u256> KnownState::tagsInExpression(KnownState::Id _expressionId)
{
	if (m_tagUnions.left.count(_expressionId))
		return m_tagUnions.left.at(_expressionId);
	// Might be a tag, then return the set of itself.
	ExpressionClasses::Expression expr = m_expressionClasses->representative(_expressionId);
	if (expr.item && expr.item->type() == PushTag)
		return std::set<u256>({expr.item->data()});
	else
		return std::set<u256>();
}

KnownState::Id KnownState::tagUnion(std::set<u256> _tags)
{
	if (m_tagUnions.right.count(_tags))
		return m_tagUnions.right.at(_tags);
	else
	{
		Id id = m_expressionClasses->newClass(langutil::DebugData::create());
		m_tagUnions.right.insert(make_pair(_tags, id));
		return id;
	}
}

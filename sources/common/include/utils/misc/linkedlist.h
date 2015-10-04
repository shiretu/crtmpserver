/*
 *  Copyright (c) 2010,
 *  Gavriloaie Eugen-Andrei (shiretu@gmail.com)
 *
 *  This file is part of crtmpserver.
 *  crtmpserver is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  crtmpserver is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with crtmpserver.  If not, see <http://www.gnu.org/licenses/>.
 */


#pragma once

#include "utils/logging/logging.h"

template<typename K, typename V>
struct LinkedListNode {
	LinkedListNode *pPrev;
	LinkedListNode *pNext;
	V value;
	K key;
};

template<typename K, typename V>
class LinkedList {
private:
	map<K, LinkedListNode<K, V> *> _allNodes;
	LinkedListNode<K, V> *_pHead;
	LinkedListNode<K, V> *_pTail;
	LinkedListNode<K, V> *_pCurrent;
	bool _denyNext;
	bool _denyPrev;
public:

	inline LinkedList() {
		_pHead = NULL;
		_pTail = NULL;
		_pCurrent = NULL;
		_denyNext = false;
		_denyPrev = false;
	}

	inline ~LinkedList() {
		Clear();
	}

	inline bool HasKey(const K &key) {
		return _allNodes.find(key) != _allNodes.end();
	}

	inline LinkedListNode<K, V> * AddHead(const K &key, const V &value) {
		if (MAP_HAS1(_allNodes, key)) {
			ASSERT("Item already present inside the list");
			return NULL;
		}
		LinkedListNode<K, V> *pNode = new LinkedListNode<K, V>();
		pNode->value = value;
		pNode->key = key;
		if (_pHead == NULL) {
			pNode->pNext = pNode->pPrev = NULL;
			_pHead = _pTail = pNode;
		} else {
			InsertBefore(_pHead, pNode);
		}
		_allNodes[key] = pNode;
		return pNode;
	}

	inline LinkedListNode<K, V> * AddTail(const K &key, const V &value) {
		if (MAP_HAS1(_allNodes, key)) {
			ASSERT("Item already present inside the list");
			return NULL;
		}
		LinkedListNode<K, V> *pNode = new LinkedListNode<K, V>();
		pNode->value = value;
		pNode->key = key;
		if (_pTail == NULL) {
			pNode->pNext = NULL;
			pNode->pPrev = NULL;
			_pHead = _pTail = pNode;
		} else {
			InsertAfter(_pTail, pNode);
		}
		_allNodes[key] = pNode;
		return pNode;
	}

	void Clear() {
		_pCurrent = NULL;
		_denyNext = false;
		_denyPrev = false;
		while (_pHead != NULL)
			Remove(_pHead);
		_pHead = NULL;
		_pTail = NULL;
	}

	inline size_t Size() {
		return _allNodes.size();
	}

	inline void Remove(const K &key) {
		if (!MAP_HAS1(_allNodes, key))
			return;
		Remove(_allNodes[key]);
	}

	inline void Remove(LinkedListNode<K, V> *pNode) {
		if (pNode == NULL)
			return;

		if (!MAP_HAS1(_allNodes, pNode->key))
			return;

		if (pNode->pPrev == NULL)
			_pHead = pNode->pNext;
		else
			pNode->pPrev->pNext = pNode->pNext;

		if (pNode->pNext == NULL)
			_pTail = pNode->pPrev;
		else
			pNode->pNext->pPrev = pNode->pPrev;

		_denyNext = false;
		_denyPrev = false;
		if (_pCurrent == pNode) {
			if (_pCurrent->pPrev != NULL) {
				_pCurrent = _pCurrent->pPrev;
				_denyPrev = (_pCurrent != NULL);
			} else {
				_pCurrent = _pCurrent->pNext;
				_denyNext = (_pCurrent != NULL);
			}
		}
		_allNodes.erase(pNode->key);
		delete pNode;
	}

	inline LinkedListNode<K, V> *MoveHead() {
		_denyNext = false;
		_denyPrev = false;
		return (_pCurrent = _pHead);
	}

	inline LinkedListNode<K, V> *MovePrev() {
		if (_denyPrev) {
			_denyPrev = false;
			return _pCurrent;
		}
		return (_pCurrent = (_pCurrent != NULL) ? _pCurrent->pPrev : NULL);
	}

	inline LinkedListNode<K, V> *Current() {
		return _pCurrent;
	}

	inline LinkedListNode<K, V> *MoveNext() {
		if (_denyNext) {
			_denyNext = false;
			return _pCurrent;
		}
		return (_pCurrent = (_pCurrent != NULL) ? _pCurrent->pNext : NULL);
	}

	inline LinkedListNode<K, V> *MoveTail() {
		_denyNext = false;
		_denyPrev = false;
		return (_pCurrent = _pTail);
	}

private:

	inline void InsertAfter(LinkedListNode<K, V> *pPosition, LinkedListNode<K, V> *pNode) {
		pNode->pPrev = pPosition;
		pNode->pNext = pPosition->pNext;
		if (pPosition->pNext == NULL)
			_pTail = pNode;
		else
			pPosition->pNext->pPrev = pNode;
		pPosition->pNext = pNode;
	}

	inline void InsertBefore(LinkedListNode<K, V> *pPosition, LinkedListNode<K, V> *pNode) {
		pNode->pPrev = pPosition->pPrev;
		pNode->pNext = pPosition;
		if (pPosition->pPrev == NULL)
			_pHead = pNode;
		else
			pPosition->pPrev->pNext = pNode;
		pPosition->pPrev = pNode;
	}
};

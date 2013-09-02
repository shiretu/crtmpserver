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


#ifndef _LINKEDLIST_H
#define	_LINKEDLIST_H

#include <stdlib.h>

template<typename T>
struct LinkedListNode {
	LinkedListNode<T> *pPrev;
	LinkedListNode<T> *pNext;
	T info;
};

template<typename T>
LinkedListNode<T> *AddLinkedList(LinkedListNode<T> *pTo, T info, bool after) {
	LinkedListNode<T> *pResult = new LinkedListNode<T>;
	pResult->pNext = NULL;
	pResult->pPrev = NULL;
	pResult->info = info;

	if (pTo != NULL) {
		if (after) {
			if (pTo->pNext != NULL) {
				pTo->pNext->pPrev = pResult;
				pResult->pNext = pTo->pNext;
				pTo->pNext = pResult;
				pResult->pPrev = pTo;
			} else {
				pTo->pNext = pResult;
				pResult->pPrev = pTo;
			}

		} else {
			if (pTo->pPrev != NULL) {
				pTo->pPrev->pNext = pResult;
				pResult->pPrev = pTo->pPrev;
				pTo->pPrev = pResult;
				pResult->pNext = pTo;
			} else {
				pTo->pPrev = pResult;
				pResult->pNext = pTo;
			}
		}
	}

	return pResult;
}

template<typename T>
LinkedListNode<T> *FirstLinkedList(LinkedListNode<T> *pNode) {
	while (pNode != NULL) {
		if (pNode->pPrev == NULL)
			return pNode;
		pNode = pNode->pPrev;
	}
	return NULL;
}

template<typename T>
LinkedListNode<T> *LastLinkedList(LinkedListNode<T> *pNode) {
	while (pNode != NULL) {
		if (pNode->pNext == NULL)
			return pNode;
		pNode = pNode->pNext;
	}
	return NULL;
}

template<typename T>
LinkedListNode<T> *RemoveLinkedList(LinkedListNode<T> *pNode) {
	LinkedListNode<T> *pPrev = pNode->pPrev;
	LinkedListNode<T> *pNext = pNode->pNext;
	if (pPrev != NULL)
		pPrev->pNext = pNext;
	if (pNext != NULL)
		pNext->pPrev = pPrev;
	delete pNode;
	if (pPrev != NULL)
		return pPrev;
	return pNext;
}

template<typename T>
class LinkedList {
private:
	LinkedListNode<T> *_pHead;
	LinkedListNode<T> *_pCurrent;
	LinkedListNode<T> *_pTail;
	uint32_t _size;
public:

	LinkedList() {
		_pHead = NULL;
		_pCurrent = NULL;
		_pTail = NULL;
		_size = 0;
	}

	virtual ~LinkedList() {
		LinkedListNode<T> *pTemp = _pTail;
		while (pTemp != NULL)
			pTemp = RemoveLinkedList<T>(pTemp);
		_pHead = NULL;
		_pCurrent = NULL;
		_pTail = NULL;
		_size = 0;
	}

	LinkedListNode<T> *Head() {
		return _pHead;
	}

	LinkedListNode<T> *Current() {
		return _pCurrent;
	}

	LinkedListNode<T> *Tail() {
		return _pTail;
	}

	uint32_t Size() {
		return _size;
	}

	LinkedListNode<T> *PushHead(T info) {
		_pHead = AddLinkedList(_pHead, info, false);
		_size++;
		if (_pCurrent == NULL)
			_pCurrent = _pHead;
		if (_pTail == NULL)
			_pTail = _pHead;
		return _pHead;
	}

	LinkedListNode<T> *PushCurrent(T info, bool after) {
		if (_pHead == NULL)
			return PushHead(info);
		_pCurrent = AddLinkedList(_pCurrent, info, after);
		_size++;
		_pHead = FirstLinkedList(_pHead);
		_pTail = LastLinkedList(_pTail);
		return _pCurrent;
	}

	LinkedListNode<T> *PushTail(T info) {
		if (_pHead == NULL)
			return PushHead(info);
		_size++;
		return (_pTail = AddLinkedList(_pTail, info, true));
	}

	LinkedListNode<T> *Next() {
		return (_pCurrent != NULL && _pCurrent->pNext != NULL) ? (_pCurrent = _pCurrent->pNext) : NULL;
	}

	LinkedListNode<T> *Prev() {
		return (_pCurrent != NULL && _pCurrent->pPrev != NULL) ? (_pCurrent = _pCurrent->pPrev) : NULL;
	}

	LinkedListNode<T> *Begin() {
		return _pCurrent = _pHead;
	}

	LinkedListNode<T> *End() {
		return _pCurrent = _pTail;
	}

	LinkedListNode<T> *Remove() {
		if (_pCurrent == NULL)
			return NULL;
		_pCurrent = RemoveLinkedList(_pCurrent);
		_pHead = FirstLinkedList(_pCurrent);
		_pTail = LastLinkedList(_pCurrent);
		_size--;
		return _pCurrent;
	}
};


#endif	/* _LINKEDLIST_H */



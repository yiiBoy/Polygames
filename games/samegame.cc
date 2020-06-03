/**
 * Copyright (c) Facebook, Inc. and its affiliates. All Rights Reserved.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "samegame.h"

#include <algorithm>
#include <cassert>
#include <stack>
#include <sstream>

Samegame::Board::Board(int nbI, int nbJ, int nbColors, std::function<int()> fillFunc) :
 _nbI(nbI),
 _nbJ(nbJ),
 _nbColors(nbColors),
 _estimatedMaxScore(pow(_nbI*_nbJ/double(_nbColors), 2.0)),
 _dataColors(nbI*nbJ),
 _dataGroups(nbI*nbJ),
 _lastIs(nbJ, nbI-1),
 _lastJs(nbI, nbJ-1),
 _score(0.0)
{
 // _dataColors
 std::generate(_dataColors.begin(), _dataColors.end(), fillFunc);
 for (int c : _dataColors) {
  assert(c >= 0);
  assert(c < _nbColors);
 }

 // _dataGroups, _groupSizes
 _groupSizes.reserve(nbI*nbJ);
 findGroups();

 // _moves
 _moves.reserve(nbI*nbJ);
 findMoves();
}

Samegame::Board::Board() :
 _nbI(15),
 _nbJ(15),
 _nbColors(5),
 _estimatedMaxScore(pow(_nbI*_nbJ/double(_nbColors), 2.0)),
 _dataColors(_nbI*_nbJ),
 _dataGroups(_nbI*_nbJ),
 _lastIs(_nbJ, _nbI-1),
 _lastJs(_nbI, _nbJ-1),
 _score(0.0)
{
}

void Samegame::Board::reset(int iDataset) {

 // _dataColors
 assert(iDataset >= 0);
 assert(iDataset < 20);
 for (int i=0; i<_nbI; ++i)
  for (int j=0; j<_nbJ; ++j)
   _dataColors[i*_nbJ+j] = DATASETS[_nbI*_nbJ*iDataset + (_nbI-i-1)*_nbJ + j];

 std::fill(_lastIs.begin(), _lastIs.end(), _nbI-1);
 std::fill(_lastJs.begin(), _lastJs.end(), _nbJ-1);
 _score = 0.0;

 // _dataGroups, _groupSizes
 _groupSizes.reserve(_nbI*_nbJ);
 findGroups();

 // _moves
 _moves.reserve(_nbI*_nbJ);
 findMoves();
}

int Samegame::Board::getNbI() const {
 return _nbI;
}

int Samegame::Board::getNbJ() const {
 return _nbJ;
}

int Samegame::Board::getNbColors() const {
 return _nbColors;
}

int Samegame::Board::isValid(int i, int j) const {
 return i >= 0 and i < _nbI and j >= 0 and j < _nbJ;
}

int Samegame::Board::dataColors(int i, int j) const {
 return _dataColors[ind(i, j)];
}

double Samegame::Board::getScore() const {
 return _score;
}

const std::vector<Samegame::Board::Move> & Samegame::Board::getMoves() const {
 return _moves;
}

bool Samegame::Board::isTerminated() const {
 return _moves.empty();
}

bool Samegame::Board::play(int i, int j) {
 for (int n=0; n<int(_moves.size()); ++n) {
  const Move & m = _moves[n];
  if (m._i == i and m._j == j) {
   play(n);
   return true;
  }
 }
 return false;
}

void Samegame::Board::play(int n) {
 assert(n >= 0);
 assert(n < int(_moves.size()));

 auto m = _moves[n];
 int k = ind(m._i, m._j);

 int group = _dataGroups[k];
 assert(group >= 0);
 assert(group < int(_dataGroups.size()));

 _score += m._eval;

 // compute i-contractions
 for (int j=0; j<_nbJ; ++j) {
  int i=0;
  while (i <= _lastIs[j]) {
   if (_dataGroups[ind(i, j)] == group)
    contractI(i, j);
   else
    ++i;
  }
 }

 // compute j-contractions
 {
  int j = 0;
  while (j <= _lastJs[0]) {
   if (_lastIs[j] == -1)
    contractJ(j);
   else
    ++j;
  }
 }

 // update groups and moves
 findGroups();
 findMoves();
}

void Samegame::Board::findGroups() {
 std::fill(_dataGroups.begin(), _dataGroups.end(), -1);
 _groupSizes.clear();
 for (int i=0; i<_nbI; ++i)
  for (int j=0; j<_nbJ; ++j)
   buildGroup(i, j);
}

void Samegame::Board::buildGroup(int i0, int j0) {
 assert(isValid(i0, j0));

 int k0 = ind(i0, j0);
 int color0 = _dataColors[k0];

 if (_dataGroups[k0] == -1 and color0 != -1) {
  // init group
  int group = _groupSizes.size();
  _groupSizes.push_back(0);

  // init spreading
  std::stack<std::pair<int,int>> cellsToSee;
  cellsToSee.push({i0, j0});
  // spread group
  while (not cellsToSee.empty()) {
   auto p = cellsToSee.top();
   cellsToSee.pop();
   int i = p.first;
   int j = p.second;
   int k = ind(i, j);
   if (_dataColors[k] == color0 and _dataGroups[k] == -1) {
    _dataGroups[k] = group;
    ++_groupSizes[group];
    if (isValid(i-1, j)) cellsToSee.push({i-1, j});
    if (isValid(i+1, j)) cellsToSee.push({i+1, j});
    if (isValid(i, j-1)) cellsToSee.push({i, j-1});
    if (isValid(i, j+1)) cellsToSee.push({i, j+1});
   }
  }
 }
}

void Samegame::Board::findMoves() {
 _moves.clear();
 for (int i=0; i<_nbI; ++i) {
  for (int j=0; j<_nbJ; ++j) {
   int color = _dataColors[ind(i, j)];
   if (color != -1) {
    int group = _dataGroups[ind(i, j)];
    assert(group >= 0);
    assert(group < int(_groupSizes.size()));
    int groupSize = _groupSizes[group];
    if (groupSize > 1) {
     double groupSize2 = std::max(0, groupSize-2);
     double eval = groupSize2 * groupSize2 / _estimatedMaxScore;
     _moves.push_back({i, j, color, eval});
    }
   }
  }
 }
}

void Samegame::Board::contractJ(int j0) {
 // do not update _dataGroups

 assert(_lastIs[j0] == -1);

 // update _lastIs
 for (int j=j0; j<_lastJs[0]; ++j)
  _lastIs[j] = _lastIs[j+1];
 _lastIs[_lastJs[0]] = -1;

 // update _dataColors and _lastJs
 for (int i=0; i<_nbI; ++i) {
  if (_lastJs[i] >= j0) {
   // move data to j-1
   for (int j=j0; j<_lastJs[i]; ++j)
    _dataColors[ind(i, j)] = _dataColors[ind(i, j+1)];

   // update last j
   _dataColors[ind(i, _lastJs[i])] = -1;
   --_lastJs[i];
  }
 }
}

void Samegame::Board::contractI(int i0, int j0) {

 assert(_lastIs[j0] >= 0);

 // move data to i-1
 for (int i=i0; i<_lastIs[j0]; ++i) {
  _dataColors[ind(i, j0)] = _dataColors[ind(i+1, j0)];
  _dataGroups[ind(i, j0)] = _dataGroups[ind(i+1, j0)];
 }

 // reset last i
 _dataColors[ind(_lastIs[j0], j0)] = -1;
 _dataGroups[ind(_lastIs[j0], j0)] = -1;
 --_lastIs[j0];
}

int Samegame::Board::ind(int i, int j) const {
 assert(isValid(i, j));
 return i * _nbJ + j;
}

// TODO check data ordering
const std::vector<int> Samegame::Board::DATASETS {
 3, 1, 1, 4, 1, 0, 4, 0, 4, 4, 1, 1, 0, 2, 3, 
 3, 3, 2, 0, 4, 4, 1, 3, 1, 2, 0, 0, 4, 0, 4, 
 0, 2, 3, 4, 3, 0, 3, 0, 0, 3, 4, 4, 1, 1, 1, 
 2, 3, 4, 0, 2, 3, 0, 2, 4, 4, 4, 3, 0, 2, 3, 
 1, 2, 1, 3, 1, 2, 0, 1, 2, 1, 0, 3, 4, 0, 1, 
 0, 4, 4, 3, 0, 3, 4, 2, 2, 2, 0, 2, 3, 4, 0, 
 2, 4, 3, 4, 2, 3, 1, 1, 1, 3, 4, 1, 0, 3, 1, 
 1, 0, 0, 4, 0, 3, 1, 2, 1, 0, 4, 1, 3, 3, 1, 
 1, 3, 3, 2, 0, 4, 3, 1, 3, 0, 4, 1, 0, 0, 3, 
 0, 3, 3, 4, 2, 3, 0, 0, 2, 1, 2, 3, 4, 0, 1, 
 0, 4, 1, 2, 0, 1, 3, 4, 3, 3, 4, 1, 4, 0, 4, 
 2, 2, 3, 1, 0, 4, 0, 1, 2, 4, 1, 3, 3, 0, 1, 
 3, 3, 0, 2, 3, 2, 1, 4, 3, 1, 3, 0, 2, 1, 3, 
 1, 0, 3, 2, 1, 4, 4, 4, 4, 0, 4, 2, 1, 3, 4, 
 1, 0, 1, 0, 1, 1, 2, 2, 1, 0, 0, 1, 4, 3, 2, 

 3, 3, 0, 1, 0, 2, 1, 2, 3, 2, 3, 1, 1, 1, 0, 
 4, 1, 3, 4, 0, 3, 3, 2, 2, 4, 0, 2, 4, 0, 0, 
 2, 3, 2, 2, 0, 3, 1, 0, 4, 4, 0, 2, 4, 0, 4, 
 0, 3, 4, 4, 2, 2, 1, 3, 3, 1, 3, 0, 3, 3, 4, 
 0, 0, 2, 1, 2, 1, 3, 4, 3, 2, 1, 2, 3, 1, 4, 
 1, 2, 4, 2, 0, 0, 0, 1, 1, 1, 0, 0, 2, 4, 4, 
 1, 0, 3, 3, 3, 2, 1, 0, 4, 2, 4, 1, 4, 3, 0, 
 4, 4, 3, 3, 0, 2, 3, 3, 4, 3, 0, 3, 0, 0, 4, 
 3, 3, 3, 1, 4, 3, 3, 3, 0, 4, 2, 0, 3, 2, 0, 
 2, 4, 1, 1, 1, 1, 4, 0, 0, 3, 0, 4, 0, 4, 3, 
 3, 3, 0, 1, 4, 1, 2, 1, 1, 0, 3, 4, 2, 1, 0, 
 2, 2, 3, 3, 2, 0, 4, 3, 3, 4, 0, 4, 3, 3, 1, 
 0, 1, 3, 2, 1, 2, 1, 1, 0, 2, 4, 1, 4, 0, 3, 
 4, 1, 4, 0, 2, 1, 3, 1, 3, 1, 4, 0, 1, 0, 3, 
 1, 3, 2, 3, 2, 2, 4, 2, 2, 4, 3, 0, 3, 1, 1, 

 4, 2, 4, 3, 1, 0, 3, 3, 2, 2, 4, 3, 1, 4, 2, 
 3, 0, 3, 4, 0, 3, 3, 3, 2, 4, 4, 3, 1, 3, 3, 
 2, 0, 4, 4, 0, 1, 2, 2, 2, 3, 4, 0, 4, 4, 0, 
 0, 4, 3, 0, 0, 2, 4, 2, 1, 2, 0, 3, 2, 4, 2, 
 0, 2, 0, 2, 0, 1, 1, 3, 2, 1, 1, 2, 3, 4, 0, 
 1, 0, 1, 0, 4, 3, 3, 3, 4, 2, 2, 2, 3, 4, 1, 
 2, 3, 4, 3, 4, 2, 2, 4, 2, 4, 3, 4, 4, 0, 1, 
 4, 2, 3, 2, 2, 0, 1, 2, 4, 3, 3, 0, 0, 2, 1, 
 3, 4, 4, 3, 0, 4, 3, 4, 1, 0, 0, 2, 1, 4, 3, 
 4, 0, 1, 3, 1, 0, 2, 3, 0, 2, 0, 2, 3, 0, 1, 
 4, 2, 0, 0, 0, 2, 2, 1, 0, 2, 3, 1, 1, 3, 1, 
 0, 3, 1, 1, 3, 3, 2, 1, 2, 0, 0, 4, 2, 4, 1, 
 2, 1, 4, 4, 4, 0, 3, 3, 4, 2, 0, 0, 2, 0, 0, 
 1, 0, 4, 4, 0, 1, 3, 2, 4, 0, 4, 2, 0, 0, 1, 
 2, 2, 2, 2, 3, 3, 0, 4, 3, 3, 4, 0, 4, 1, 2, 

 4, 2, 2, 4, 1, 3, 3, 2, 4, 0, 4, 2, 3, 4, 2, 
 2, 0, 2, 1, 2, 1, 0, 1, 2, 1, 1, 3, 0, 4, 2, 
 0, 2, 3, 2, 0, 0, 4, 1, 0, 4, 3, 0, 0, 3, 2, 
 2, 2, 3, 1, 1, 0, 0, 1, 0, 1, 1, 4, 3, 0, 0, 
 4, 2, 0, 4, 2, 2, 0, 3, 0, 0, 2, 2, 1, 4, 2, 
 1, 4, 3, 3, 2, 3, 0, 4, 4, 0, 0, 2, 2, 3, 0, 
 2, 1, 1, 4, 1, 0, 1, 0, 4, 4, 1, 0, 4, 1, 3, 
 3, 3, 0, 2, 1, 3, 1, 1, 4, 0, 2, 3, 3, 3, 3, 
 2, 3, 3, 1, 3, 1, 0, 4, 1, 0, 1, 2, 3, 0, 4, 
 3, 2, 1, 1, 3, 4, 0, 2, 4, 2, 4, 2, 0, 2, 0, 
 0, 3, 0, 1, 4, 0, 0, 0, 4, 2, 1, 0, 2, 4, 0, 
 2, 0, 1, 4, 2, 3, 1, 4, 2, 0, 1, 0, 3, 4, 2, 
 0, 4, 2, 0, 3, 4, 4, 3, 1, 1, 3, 4, 2, 1, 4, 
 4, 2, 4, 0, 4, 3, 0, 2, 2, 4, 1, 4, 3, 4, 1, 
 4, 3, 2, 2, 2, 1, 1, 2, 3, 3, 1, 2, 0, 3, 2, 

 3, 4, 4, 3, 2, 3, 2, 1, 3, 4, 1, 2, 3, 3, 2, 
 2, 0, 2, 0, 3, 1, 0, 3, 1, 1, 2, 1, 4, 3, 4, 
 1, 3, 1, 0, 3, 1, 3, 2, 3, 4, 0, 0, 1, 4, 1, 
 0, 2, 1, 0, 2, 2, 2, 4, 1, 0, 4, 4, 3, 3, 2, 
 2, 3, 1, 3, 0, 4, 0, 2, 3, 0, 1, 4, 4, 2, 3, 
 3, 1, 3, 3, 2, 3, 0, 1, 0, 4, 3, 4, 0, 1, 4, 
 4, 4, 4, 2, 2, 3, 0, 0, 0, 1, 0, 1, 2, 1, 3, 
 2, 1, 3, 4, 4, 0, 4, 1, 0, 4, 0, 1, 2, 1, 3, 
 3, 4, 3, 1, 2, 0, 1, 3, 3, 0, 1, 4, 2, 0, 0, 
 2, 3, 0, 1, 2, 4, 3, 3, 0, 1, 1, 2, 2, 3, 3, 
 4, 4, 1, 0, 3, 3, 4, 4, 2, 2, 4, 2, 0, 3, 0, 
 3, 1, 0, 4, 3, 2, 0, 2, 3, 1, 4, 3, 1, 2, 2, 
 2, 2, 3, 0, 2, 4, 1, 3, 0, 3, 2, 1, 3, 4, 2, 
 2, 4, 3, 1, 3, 0, 3, 2, 0, 4, 3, 2, 2, 3, 4, 
 0, 4, 2, 2, 2, 3, 2, 0, 1, 1, 4, 0, 1, 3, 3, 

 2, 4, 2, 0, 4, 2, 2, 3, 1, 0, 1, 3, 4, 2, 0, 
 2, 3, 3, 2, 3, 1, 3, 3, 0, 1, 4, 1, 0, 0, 1, 
 0, 4, 3, 0, 3, 1, 3, 3, 3, 1, 0, 2, 4, 2, 1, 
 3, 0, 1, 0, 1, 2, 3, 0, 0, 2, 1, 1, 1, 4, 4, 
 0, 1, 1, 1, 2, 0, 2, 1, 3, 4, 2, 0, 3, 1, 0, 
 1, 1, 1, 4, 1, 1, 0, 0, 1, 1, 4, 1, 1, 2, 1, 
 3, 3, 0, 1, 1, 3, 2, 0, 0, 0, 0, 1, 2, 0, 1, 
 0, 3, 0, 3, 4, 0, 1, 1, 2, 1, 4, 2, 1, 0, 2, 
 1, 2, 2, 2, 2, 3, 4, 1, 3, 1, 4, 2, 4, 1, 1, 
 2, 2, 0, 3, 3, 0, 2, 2, 3, 3, 2, 2, 1, 0, 3, 
 2, 4, 0, 0, 4, 0, 4, 3, 4, 4, 3, 4, 1, 4, 4, 
 2, 1, 2, 3, 1, 1, 2, 2, 1, 0, 3, 1, 4, 4, 0, 
 2, 3, 2, 2, 1, 1, 4, 0, 1, 4, 4, 0, 4, 3, 3, 
 1, 1, 3, 0, 3, 1, 4, 3, 4, 1, 0, 4, 1, 1, 4, 
 0, 4, 4, 4, 2, 2, 4, 3, 1, 1, 3, 2, 4, 4, 1, 

 3, 4, 0, 3, 1, 2, 0, 1, 3, 1, 2, 4, 1, 1, 3, 
 3, 1, 4, 3, 0, 0, 1, 3, 0, 2, 0, 4, 4, 4, 4, 
 0, 4, 3, 2, 1, 1, 0, 2, 2, 1, 3, 4, 0, 2, 3, 
 2, 4, 0, 1, 3, 3, 3, 2, 2, 2, 2, 0, 2, 2, 0, 
 0, 4, 0, 0, 2, 1, 0, 1, 4, 3, 3, 3, 1, 0, 2, 
 1, 0, 4, 1, 2, 4, 4, 2, 2, 0, 0, 0, 3, 4, 4, 
 4, 2, 1, 3, 1, 2, 0, 1, 3, 4, 2, 2, 1, 3, 2, 
 1, 1, 1, 0, 3, 0, 3, 1, 3, 3, 1, 1, 2, 3, 0, 
 1, 2, 4, 3, 1, 4, 1, 1, 1, 0, 2, 3, 0, 3, 3, 
 0, 4, 1, 3, 4, 0, 4, 1, 4, 0, 4, 2, 3, 0, 1, 
 0, 4, 3, 4, 2, 4, 1, 3, 1, 3, 0, 4, 3, 0, 0, 
 3, 1, 1, 1, 0, 4, 2, 0, 3, 0, 4, 4, 2, 4, 4, 
 4, 0, 4, 3, 1, 4, 1, 3, 2, 3, 0, 1, 0, 1, 1, 
 3, 3, 4, 2, 4, 4, 2, 0, 3, 4, 3, 0, 1, 0, 3, 
 0, 2, 3, 4, 4, 2, 4, 1, 0, 0, 0, 4, 2, 4, 0, 

 3, 1, 3, 1, 4, 4, 2, 2, 0, 4, 0, 2, 2, 3, 1, 
 1, 1, 2, 3, 3, 1, 0, 2, 2, 2, 0, 2, 4, 1, 1, 
 4, 4, 1, 2, 4, 2, 1, 4, 1, 2, 3, 3, 2, 1, 4, 
 1, 0, 2, 2, 3, 4, 1, 3, 2, 2, 1, 3, 4, 3, 2, 
 3, 1, 1, 0, 0, 1, 2, 0, 3, 2, 4, 3, 4, 3, 1, 
 1, 1, 3, 0, 4, 2, 1, 3, 0, 1, 2, 4, 4, 0, 3, 
 0, 1, 1, 1, 0, 1, 2, 3, 3, 1, 0, 1, 0, 0, 3, 
 2, 3, 2, 3, 1, 1, 1, 2, 4, 0, 2, 1, 2, 3, 3, 
 0, 1, 3, 0, 4, 3, 1, 1, 4, 0, 1, 3, 0, 3, 0, 
 1, 3, 3, 0, 3, 0, 0, 0, 3, 4, 1, 3, 0, 0, 0, 
 4, 4, 2, 1, 3, 1, 0, 1, 1, 3, 1, 3, 2, 4, 3, 
 0, 3, 0, 2, 3, 1, 1, 1, 3, 3, 1, 2, 3, 2, 2, 
 3, 2, 2, 0, 3, 0, 3, 1, 0, 0, 3, 3, 2, 4, 2, 
 0, 1, 2, 2, 0, 2, 4, 4, 1, 3, 4, 3, 1, 1, 4, 
 4, 4, 3, 0, 4, 3, 3, 3, 4, 1, 3, 4, 4, 3, 1, 

 1, 3, 4, 0, 2, 1, 4, 3, 0, 0, 1, 2, 3, 1, 1, 
 0, 0, 3, 0, 3, 2, 3, 0, 1, 4, 0, 3, 3, 3, 2, 
 2, 4, 1, 2, 0, 1, 2, 1, 0, 0, 3, 1, 0, 2, 2, 
 0, 2, 1, 2, 1, 1, 0, 0, 0, 3, 3, 0, 1, 1, 3, 
 1, 4, 2, 3, 1, 3, 3, 0, 4, 2, 3, 1, 0, 4, 4, 
 2, 1, 1, 4, 1, 1, 4, 0, 4, 4, 2, 0, 0, 4, 0, 
 3, 4, 4, 3, 0, 0, 2, 0, 4, 1, 2, 4, 0, 3, 3, 
 1, 4, 0, 4, 0, 0, 3, 3, 4, 4, 0, 2, 2, 4, 4, 
 0, 1, 0, 4, 2, 3, 3, 0, 0, 2, 0, 4, 3, 4, 1, 
 3, 1, 1, 4, 2, 4, 0, 0, 2, 0, 3, 1, 2, 4, 3, 
 0, 0, 4, 2, 4, 1, 2, 0, 0, 0, 3, 0, 3, 3, 3, 
 0, 0, 1, 0, 1, 2, 2, 0, 3, 4, 3, 2, 4, 3, 4, 
 1, 1, 0, 2, 0, 4, 3, 3, 1, 1, 4, 3, 2, 4, 1, 
 0, 1, 2, 2, 3, 4, 0, 3, 1, 4, 0, 0, 3, 1, 1, 
 0, 3, 0, 0, 1, 0, 1, 1, 1, 3, 1, 2, 0, 0, 0, 

 0, 1, 3, 3, 4, 3, 4, 3, 2, 4, 4, 0, 3, 2, 1, 
 4, 0, 1, 1, 0, 0, 0, 1, 2, 0, 3, 0, 0, 2, 1, 
 1, 2, 4, 3, 0, 2, 0, 2, 3, 4, 3, 1, 2, 2, 3, 
 3, 4, 3, 0, 1, 3, 3, 2, 3, 1, 1, 0, 3, 4, 2, 
 2, 0, 0, 3, 2, 0, 2, 3, 3, 3, 0, 1, 1, 1, 1, 
 2, 4, 2, 2, 1, 4, 3, 2, 1, 4, 0, 1, 4, 4, 1, 
 0, 0, 0, 2, 2, 3, 4, 3, 2, 3, 0, 3, 4, 3, 4, 
 1, 2, 0, 4, 1, 2, 2, 4, 0, 2, 4, 2, 4, 0, 3, 
 3, 4, 3, 3, 1, 1, 0, 4, 4, 2, 1, 0, 0, 1, 3, 
 1, 2, 2, 2, 4, 3, 2, 0, 2, 1, 0, 1, 0, 1, 3, 
 2, 3, 4, 2, 1, 0, 1, 2, 3, 2, 4, 0, 2, 4, 3, 
 1, 3, 2, 4, 3, 0, 4, 4, 1, 1, 4, 1, 2, 4, 0, 
 3, 0, 2, 2, 1, 4, 3, 4, 1, 2, 2, 1, 1, 3, 1, 
 2, 0, 2, 1, 0, 4, 1, 4, 0, 3, 2, 3, 0, 2, 4, 
 0, 3, 1, 1, 0, 1, 4, 1, 4, 1, 1, 1, 0, 4, 2, 

 4, 1, 2, 0, 2, 3, 4, 1, 4, 4, 1, 4, 3, 1, 3, 
 1, 3, 1, 3, 4, 0, 3, 4, 2, 3, 3, 2, 3, 4, 1, 
 1, 3, 2, 2, 3, 4, 2, 3, 4, 0, 3, 4, 1, 2, 3, 
 1, 3, 2, 4, 0, 2, 0, 0, 1, 2, 1, 3, 4, 4, 2, 
 4, 0, 2, 2, 0, 1, 1, 0, 0, 1, 0, 2, 3, 2, 4, 
 2, 2, 0, 3, 4, 1, 0, 4, 3, 4, 4, 2, 3, 3, 4, 
 4, 4, 0, 2, 0, 3, 4, 1, 1, 4, 4, 2, 0, 1, 1, 
 3, 1, 0, 4, 1, 1, 1, 3, 2, 4, 1, 3, 2, 0, 2, 
 0, 2, 0, 0, 1, 1, 2, 0, 4, 1, 1, 0, 2, 2, 4, 
 3, 1, 0, 4, 3, 4, 3, 1, 1, 0, 0, 3, 2, 3, 4, 
 4, 4, 1, 2, 4, 0, 4, 2, 0, 3, 2, 3, 4, 0, 0, 
 2, 4, 3, 0, 1, 3, 1, 3, 1, 0, 1, 0, 0, 1, 4, 
 1, 2, 1, 2, 0, 0, 3, 0, 1, 1, 0, 2, 3, 1, 2, 
 3, 2, 0, 1, 3, 0, 2, 4, 3, 4, 4, 4, 0, 3, 0, 
 2, 3, 3, 0, 2, 2, 4, 3, 0, 2, 1, 2, 3, 2, 0, 

 1, 2, 2, 4, 2, 3, 4, 2, 4, 1, 2, 2, 3, 3, 4, 
 3, 1, 1, 4, 1, 1, 1, 1, 1, 2, 1, 1, 4, 1, 0, 
 1, 4, 1, 4, 4, 2, 1, 4, 0, 3, 4, 0, 2, 3, 3, 
 3, 3, 1, 2, 0, 3, 3, 3, 2, 4, 0, 1, 2, 3, 0, 
 4, 3, 4, 1, 3, 0, 4, 4, 3, 4, 0, 4, 0, 0, 2, 
 2, 0, 3, 1, 2, 4, 4, 4, 0, 0, 2, 3, 0, 0, 3, 
 0, 4, 0, 3, 4, 2, 1, 1, 0, 3, 3, 3, 2, 2, 1, 
 0, 2, 0, 3, 1, 4, 0, 0, 1, 2, 0, 3, 4, 1, 2, 
 3, 2, 2, 2, 1, 1, 1, 4, 3, 2, 0, 2, 4, 2, 2, 
 4, 3, 3, 0, 3, 0, 0, 4, 0, 0, 2, 2, 3, 3, 1, 
 4, 2, 3, 4, 1, 2, 3, 1, 3, 0, 4, 4, 4, 0, 2, 
 0, 1, 3, 1, 2, 3, 2, 4, 3, 3, 1, 2, 4, 0, 1, 
 4, 1, 3, 3, 1, 0, 3, 2, 0, 1, 4, 0, 2, 0, 2, 
 4, 0, 2, 4, 1, 0, 0, 4, 2, 0, 0, 4, 4, 3, 0, 
 1, 1, 1, 3, 4, 2, 3, 2, 1, 2, 0, 1, 4, 1, 0, 

 4, 0, 1, 4, 3, 3, 1, 4, 1, 2, 4, 1, 0, 0, 2, 
 0, 1, 4, 0, 3, 0, 0, 2, 4, 2, 2, 3, 3, 2, 4, 
 0, 2, 1, 0, 3, 3, 3, 0, 0, 4, 4, 3, 1, 1, 4, 
 4, 4, 2, 1, 0, 2, 4, 3, 3, 2, 2, 4, 2, 4, 0, 
 3, 0, 0, 4, 4, 2, 2, 1, 3, 4, 3, 2, 4, 2, 0, 
 0, 4, 1, 4, 4, 4, 4, 4, 1, 2, 3, 4, 2, 3, 3, 
 0, 1, 2, 0, 0, 2, 2, 1, 3, 4, 2, 0, 0, 4, 1, 
 4, 3, 3, 2, 0, 0, 1, 0, 1, 4, 3, 2, 3, 1, 1, 
 3, 4, 2, 2, 0, 2, 3, 3, 3, 0, 0, 1, 2, 1, 3, 
 1, 3, 2, 1, 2, 2, 4, 1, 1, 1, 2, 3, 1, 3, 1, 
 0, 0, 2, 1, 2, 1, 1, 4, 1, 1, 0, 2, 1, 2, 0, 
 4, 1, 2, 1, 0, 3, 1, 0, 3, 4, 0, 4, 3, 3, 2, 
 4, 3, 0, 0, 3, 4, 3, 3, 3, 3, 1, 1, 3, 2, 1, 
 0, 1, 1, 3, 0, 1, 1, 0, 4, 0, 4, 0, 2, 0, 4, 
 2, 2, 1, 4, 4, 2, 2, 0, 3, 4, 3, 0, 2, 4, 3, 

 2, 2, 4, 0, 2, 4, 0, 0, 1, 4, 0, 3, 4, 3, 3, 
 0, 4, 3, 1, 0, 3, 2, 0, 1, 2, 2, 1, 4, 4, 0, 
 2, 1, 2, 3, 3, 2, 1, 2, 3, 3, 0, 4, 2, 1, 0, 
 4, 4, 3, 3, 2, 4, 1, 0, 1, 4, 4, 0, 4, 2, 1, 
 3, 3, 0, 1, 2, 2, 3, 1, 3, 0, 1, 3, 2, 3, 3, 
 1, 2, 0, 3, 4, 0, 4, 2, 2, 2, 1, 3, 3, 3, 1, 
 4, 0, 0, 1, 1, 1, 1, 4, 3, 3, 2, 1, 3, 2, 0, 
 4, 1, 4, 4, 1, 0, 0, 2, 0, 3, 2, 2, 0, 2, 3, 
 2, 3, 3, 1, 4, 3, 0, 1, 0, 4, 4, 0, 0, 2, 1, 
 0, 1, 2, 2, 4, 3, 1, 1, 4, 4, 2, 4, 4, 2, 4, 
 2, 4, 1, 1, 0, 3, 3, 3, 0, 4, 4, 0, 0, 2, 0, 
 3, 2, 1, 3, 0, 4, 4, 2, 3, 0, 2, 1, 1, 3, 1, 
 0, 4, 3, 3, 1, 2, 0, 2, 2, 1, 2, 3, 0, 0, 1, 
 4, 3, 4, 2, 1, 1, 3, 0, 4, 1, 4, 1, 4, 2, 0, 
 2, 1, 3, 2, 0, 1, 4, 0, 1, 4, 0, 4, 0, 4, 3, 

 0, 1, 2, 1, 3, 4, 3, 2, 1, 2, 1, 2, 2, 3, 4, 
 4, 0, 0, 1, 3, 0, 4, 2, 0, 4, 4, 4, 2, 1, 1, 
 3, 0, 0, 1, 2, 1, 1, 3, 0, 0, 3, 2, 4, 0, 0, 
 4, 2, 1, 4, 4, 1, 4, 0, 0, 3, 2, 0, 2, 2, 0, 
 3, 3, 4, 2, 1, 2, 4, 1, 3, 4, 0, 4, 2, 3, 0, 
 0, 4, 4, 1, 2, 2, 1, 4, 4, 2, 3, 3, 4, 4, 1, 
 3, 1, 1, 3, 2, 2, 0, 3, 2, 3, 4, 4, 3, 2, 0, 
 2, 4, 1, 3, 2, 0, 2, 4, 4, 4, 1, 4, 4, 0, 0, 
 1, 4, 2, 1, 2, 0, 3, 3, 0, 1, 3, 3, 2, 4, 3, 
 2, 2, 3, 2, 1, 1, 0, 0, 1, 1, 3, 1, 2, 4, 3, 
 2, 1, 0, 2, 2, 0, 3, 2, 2, 1, 4, 1, 1, 4, 0, 
 0, 1, 3, 2, 1, 0, 4, 0, 0, 3, 3, 0, 3, 0, 4, 
 1, 2, 1, 3, 4, 3, 1, 1, 3, 0, 0, 4, 3, 1, 4, 
 0, 3, 3, 3, 1, 1, 4, 0, 0, 4, 2, 4, 1, 0, 3, 
 3, 0, 3, 2, 1, 4, 0, 3, 3, 1, 2, 2, 0, 4, 2, 

 2, 0, 1, 4, 4, 3, 1, 4, 2, 0, 4, 0, 4, 0, 1, 
 3, 3, 0, 2, 1, 1, 1, 4, 2, 4, 3, 4, 2, 1, 0, 
 4, 1, 4, 4, 1, 2, 1, 1, 1, 2, 3, 1, 0, 3, 3, 
 4, 4, 2, 3, 3, 0, 2, 0, 3, 2, 1, 4, 4, 1, 4, 
 1, 4, 1, 3, 3, 3, 1, 0, 2, 2, 2, 2, 2, 3, 0, 
 0, 4, 2, 3, 0, 3, 1, 0, 1, 1, 3, 1, 3, 2, 1, 
 2, 0, 2, 4, 1, 1, 2, 1, 3, 1, 1, 1, 2, 2, 2, 
 1, 3, 3, 3, 1, 1, 0, 0, 3, 3, 0, 2, 1, 1, 1, 
 0, 0, 0, 4, 4, 1, 3, 2, 4, 1, 0, 0, 3, 3, 0, 
 4, 3, 2, 3, 1, 3, 3, 3, 4, 3, 1, 2, 2, 1, 1, 
 1, 1, 1, 1, 2, 0, 2, 1, 4, 1, 3, 1, 1, 1, 2, 
 0, 1, 2, 1, 0, 4, 0, 2, 3, 1, 0, 0, 0, 1, 0, 
 0, 1, 1, 4, 3, 3, 4, 4, 0, 0, 1, 0, 1, 2, 4, 
 3, 2, 2, 1, 1, 4, 0, 2, 1, 0, 1, 0, 4, 4, 4, 
 4, 0, 1, 1, 2, 2, 4, 3, 4, 1, 2, 4, 4, 3, 4, 

 0, 2, 2, 2, 4, 1, 2, 0, 4, 0, 2, 3, 0, 2, 2, 
 4, 4, 4, 2, 2, 2, 1, 2, 3, 2, 3, 0, 0, 2, 1, 
 3, 2, 0, 1, 2, 3, 2, 4, 3, 1, 0, 4, 2, 0, 2, 
 2, 1, 2, 0, 0, 2, 2, 3, 4, 3, 2, 2, 2, 1, 3, 
 0, 2, 0, 3, 2, 0, 2, 1, 2, 2, 2, 3, 3, 0, 2, 
 3, 1, 0, 4, 3, 0, 1, 1, 0, 3, 0, 0, 2, 3, 4, 
 0, 3, 4, 1, 3, 4, 3, 1, 1, 3, 3, 1, 2, 1, 3, 
 4, 2, 3, 1, 1, 0, 3, 3, 4, 4, 1, 1, 4, 4, 3, 
 3, 0, 4, 1, 1, 1, 3, 3, 1, 4, 1, 1, 4, 4, 2, 
 2, 1, 3, 0, 2, 2, 4, 2, 4, 2, 1, 1, 2, 2, 0, 
 0, 1, 3, 2, 4, 4, 0, 0, 0, 4, 2, 2, 4, 2, 2, 
 3, 0, 1, 2, 4, 1, 0, 3, 3, 1, 0, 4, 0, 2, 2, 
 4, 2, 1, 4, 2, 2, 2, 2, 0, 2, 1, 0, 4, 3, 0, 
 0, 4, 3, 2, 0, 2, 3, 2, 4, 2, 1, 1, 1, 3, 4, 
 4, 2, 4, 4, 0, 2, 0, 1, 3, 4, 2, 4, 2, 3, 1, 

 0, 2, 2, 4, 4, 3, 3, 3, 3, 0, 4, 3, 0, 2, 3, 
 4, 1, 4, 4, 4, 1, 3, 1, 4, 1, 0, 3, 0, 2, 1, 
 0, 4, 1, 3, 0, 3, 1, 3, 3, 2, 4, 0, 4, 3, 2, 
 1, 2, 3, 3, 4, 2, 1, 0, 2, 3, 3, 3, 2, 3, 4, 
 0, 4, 1, 4, 1, 1, 2, 3, 0, 2, 1, 3, 1, 0, 2, 
 3, 1, 3, 4, 1, 3, 3, 1, 4, 3, 3, 2, 4, 4, 0, 
 0, 2, 4, 4, 1, 0, 0, 3, 2, 3, 2, 3, 3, 3, 2, 
 0, 1, 4, 3, 3, 1, 2, 1, 3, 2, 3, 1, 2, 0, 2, 
 0, 2, 0, 2, 3, 1, 3, 4, 1, 1, 0, 2, 1, 4, 1, 
 0, 3, 4, 0, 0, 2, 3, 2, 4, 3, 3, 0, 0, 0, 3, 
 2, 4, 0, 2, 2, 0, 3, 1, 0, 2, 3, 2, 3, 2, 3, 
 4, 3, 1, 1, 4, 3, 1, 1, 3, 1, 3, 0, 4, 1, 3, 
 4, 2, 1, 1, 3, 3, 0, 3, 0, 4, 0, 3, 4, 3, 0, 
 1, 2, 4, 4, 2, 4, 3, 4, 1, 4, 3, 0, 0, 0, 2, 
 4, 3, 3, 2, 3, 3, 0, 0, 1, 1, 2, 3, 3, 4, 2, 

 4, 4, 1, 4, 1, 1, 2, 4, 0, 3, 3, 0, 1, 2, 0, 
 4, 1, 1, 4, 3, 1, 1, 2, 2, 3, 0, 2, 4, 0, 3, 
 3, 2, 3, 4, 0, 1, 4, 0, 2, 4, 4, 1, 2, 3, 0, 
 0, 4, 3, 4, 0, 2, 4, 0, 4, 4, 4, 1, 4, 3, 4, 
 4, 0, 1, 4, 2, 0, 1, 2, 4, 3, 0, 1, 4, 4, 3, 
 3, 1, 4, 2, 2, 1, 2, 1, 2, 2, 4, 2, 1, 2, 2, 
 2, 4, 2, 0, 4, 0, 3, 4, 3, 0, 2, 3, 3, 3, 0, 
 3, 4, 3, 4, 0, 0, 0, 1, 1, 4, 1, 2, 1, 3, 3, 
 4, 4, 4, 1, 2, 4, 2, 4, 0, 1, 4, 1, 4, 4, 3, 
 0, 4, 2, 2, 4, 0, 3, 1, 3, 2, 4, 1, 4, 4, 4, 
 0, 0, 2, 1, 4, 0, 2, 4, 3, 4, 0, 0, 4, 3, 0, 
 4, 2, 1, 0, 2, 2, 4, 2, 2, 2, 3, 3, 1, 0, 0, 
 4, 1, 3, 3, 4, 3, 1, 3, 2, 1, 1, 3, 1, 4, 2, 
 1, 1, 4, 0, 4, 3, 3, 2, 0, 2, 4, 3, 1, 4, 0, 
 3, 3, 1, 4, 1, 4, 0, 4, 0, 4, 3, 0, 4, 4, 0, 

 3, 0, 1, 3, 3, 0, 0, 1, 0, 0, 2, 4, 0, 0, 1, 
 1, 2, 2, 3, 2, 2, 0, 4, 0, 2, 3, 2, 2, 2, 1, 
 3, 1, 0, 0, 0, 0, 4, 4, 1, 3, 1, 3, 2, 0, 4, 
 0, 1, 0, 2, 0, 3, 4, 3, 2, 3, 0, 2, 0, 3, 4, 
 2, 3, 2, 2, 0, 3, 3, 0, 0, 3, 0, 3, 4, 1, 1, 
 0, 3, 3, 2, 0, 4, 1, 2, 4, 1, 2, 4, 4, 1, 0, 
 3, 2, 4, 0, 4, 1, 4, 3, 2, 1, 1, 4, 0, 0, 2, 
 1, 4, 1, 3, 0, 4, 0, 3, 2, 3, 2, 0, 0, 0, 1, 
 0, 0, 0, 1, 4, 2, 1, 0, 4, 4, 4, 3, 1, 0, 4, 
 3, 3, 3, 1, 0, 3, 1, 2, 0, 2, 4, 3, 4, 1, 1, 
 1, 1, 1, 3, 0, 2, 2, 3, 0, 4, 3, 4, 4, 1, 1, 
 0, 2, 0, 0, 2, 0, 0, 1, 3, 0, 2, 3, 0, 2, 4, 
 4, 3, 3, 2, 4, 0, 0, 0, 4, 3, 1, 0, 4, 1, 2, 
 2, 2, 3, 2, 0, 4, 2, 0, 0, 4, 1, 4, 4, 0, 1, 
 3, 4, 1, 4, 4, 0, 0, 0, 0, 1, 0, 2, 1, 0, 0, 
};

#include "sage3basic.h"
#include "IntervalPropertyState.h"

using namespace SPRAY;

SPRAY::IntervalPropertyState::IntervalPropertyState() {
  setBot();
}

void SPRAY::IntervalPropertyState::toStream(ostream& os, VariableIdMapping* vim) {
  if(isBot()) {
    os<<"bot";
  } else {
    os<<"{";
    for(IntervalMapType::iterator i=intervals.begin();i!=intervals.end();++i) {
      if(i!=intervals.begin())
        os<<", ";
      VariableId varId=(*i).first;
      SPRAY::NumberIntervalLattice niElem=(*i).second;
      if(vim)
        os<<vim->variableName(varId)<<"->"<<niElem.toString();
      else
        os<<varId.toString()<<"->"<<niElem.toString();
    }
    os<<"}";
  }
}

bool SPRAY::IntervalPropertyState::approximatedBy(Lattice& other0) {
  IntervalPropertyState* other=dynamic_cast<IntervalPropertyState*> (&other0);
  ROSE_ASSERT(other);
  if(isBot())
    return true;
  if(!isBot()&&other->isBot())
    return false;
  for(IntervalMapType::iterator i=intervals.begin();i!=intervals.end();++i) {
    VariableId varId=(*i).first;
    if(!SPRAY::NumberIntervalLattice::isSubIntervalOf(intervals[varId],other->intervals[varId]))
      return false;
  }
  return true;
}

bool SPRAY::IntervalPropertyState::approximatedByAsymmetric(Lattice& other) {
  return approximatedBy(other);
}

// schroder3 (2016-08-05): Merges the "other0" IntervalPropertyState into this IntervalPropertyState.
//  The interval of each variable in this state is joined with the interval of the corresponding
//  variable in the "other0" state.
void SPRAY::IntervalPropertyState::combineInternal(Lattice& other0, JoinMode joinMode) {
  IntervalPropertyState* other=dynamic_cast<IntervalPropertyState*> (&other0);
  ROSE_ASSERT(other!=0);
  if(isBot()&&other->isBot())
    return;
  if(!isBot()&&other->isBot())
    return;
  if(isBot()&&!other->isBot())
    _bot=false;
  for(IntervalMapType::iterator i=intervals.begin();i!=intervals.end();++i) {
    VariableId varId=(*i).first;
    if(other->intervals.find(varId)==other->intervals.end()) {
      // ps's variable is not in other's ps
      // nothing to do - assume bot in this case
    }else {
      intervals[varId].join(other->intervals[varId], joinMode);
    }
  }
  for(IntervalMapType::iterator i=other->intervals.begin();i!=other->intervals.end();++i) {
    VariableId varId=(*i).first;
    if(intervals.find(varId)==intervals.end()) {
      // other ps's variable is not in this ps yet
      intervals[varId]=(*i).second;
    }else {
      // schroder3 (2016-08-05): This should already been handled in the upper for-loop and this
      //  should therefore never change anything:
      NumberIntervalLattice previous = intervals[varId].getCopy();
      intervals[varId].join(other->intervals[varId], joinMode);
      ROSE_ASSERT(intervals[varId] == previous);
    }
  }
}

void SPRAY::IntervalPropertyState::combine(Lattice& other0){
  // schroder3 (2016-08-05): Use the exact join:
  combineInternal(other0, JM_Exact);
}

void SPRAY::IntervalPropertyState::combineAsymmetric(Lattice& other0){
  // schroder3 (2016-08-05): Use the asymmetric join:
  combineInternal(other0, JM_InfinityAsymmetric);
}

// adds integer variable
void SPRAY::IntervalPropertyState::addVariable(VariableId varId) {
  setVariable(varId,NumberIntervalLattice());
}

// assign integer variable
void SPRAY::IntervalPropertyState::setVariable(VariableId varId, NumberIntervalLattice num) {
  _bot=false;
  intervals[varId]=num;
}

// assign integer variable
NumberIntervalLattice SPRAY::IntervalPropertyState::getVariable(VariableId varId) {
  return intervals[varId];
}

void SPRAY::IntervalPropertyState::setEmptyState() {
  for(IntervalMapType::iterator i=intervals.begin();i!=intervals.end();++i) {
    intervals[(*i).first]=NumberIntervalLattice();
  }
  _bot=false;
}

#if 0
  // adds pointer variable
  void SPRAY::IntervalPropertyState::addPointerVariable(VariableId);
  // adds array elements for indices 0 to number-1
  void SPRAY::IntervalPropertyState::addArrayElements(VariableId,int number);
#endif

VariableIdSet SPRAY::IntervalPropertyState::allVariableIds() {
  VariableIdSet set;
  for(IntervalMapType::iterator i=intervals.begin();i!=intervals.end();++i) {
    set.insert((*i).first);
  }
  return set;
}

void SPRAY::IntervalPropertyState::topifyVariableSet(VariableIdSet varIdSet) {
  for(VariableIdSet::iterator i=varIdSet.begin();i!=varIdSet.end();++i) {
    intervals[*i]=SPRAY::NumberIntervalLattice::top();
  }
}
void SPRAY::IntervalPropertyState::topifyAllVariables() {
  for(IntervalMapType::iterator i=intervals.begin();i!=intervals.end();++i) {
    intervals[(*i).first]=SPRAY::NumberIntervalLattice::top();
  }
}

bool SPRAY::IntervalPropertyState::variableExists(VariableId varId) {
  return intervals.find(varId)!=intervals.end();
}

bool SPRAY::IntervalPropertyState::removeVariable(VariableId varId) {
  // for STL map numErased can only be 0 or 1.
  size_t numErased=intervals.erase(varId);
  return numErased==1;
}

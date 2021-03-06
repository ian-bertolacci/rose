// WE ASSUME THAT EACH CFG NODE CONTAINS ONLY ONE NODE STATE.
// IN PARTICULAR, WE ASSUME THAT IF THERE IS AN ANALYSIS SPLIT
// THE PARTITIONS WILL CORRESPOND TO SUCCEEDING CFG NODES, NOT 
// TO A SINGLE NEXT NODE STATE ASSOCIATED WITH THE SAME CFG NODE.

#include "partitionedAnalysis.h"
//#include "ConstrGraph.h"

/***************************
 *** PartitionedAnalysis ***
 ***************************/

// Creates a IntraPartitionDataflow, starting the analysis with the given
// master analysis partition and creating an initial split that only contains the master.
PartitionedAnalysis::PartitionedAnalysis(IntraPartitionDataflow* intraFactory)
{
        ROSE_ASSERT(intraFactory->getParent() == this);
        this->intraFactory = intraFactory;
}

// Create the initial partition state for analyzing some function
void PartitionedAnalysis::initMaster()
{
        IntraPartitionDataflow* master = intraFactory->copy();
        // Create a new active partition
        activeParts.insert(master);

        // Inform this partition about its parent PartitionedAnalysis object`
        //master->setParent(this);
        
        // Create a new partition split that corresponds to just the master partition
        partSplit* newSplit = new partSplit(master);
        list<partSplit*> splitsList;
        splitsList.push_back(newSplit);
        parts2splits[master] = splitsList;
        parts2chkpts[master] = NULL;
}

// Returns a reference to the master dataflow analysis. At the end of the partitioned analysis,
// it is this object that owns all the dataflow state generated by the overall analysis.
IntraPartitionDataflow* PartitionedAnalysis::getMasterDFAnalysis()
{
        //printf("parts2splits() = %d\n", parts2splits.size());
        return parts2splits.begin()->first;
}

// Activates the given partition, returning true if it was previously inactive and false otherwise.
bool PartitionedAnalysis::activatePart(IntraPartitionDataflow* part)
{
        bool modified = joinParts.find(part) != joinParts.end();
        
        if(modified)
        {
                joinParts.erase(part);
                activeParts.insert(part);
        }
        return modified;
}

// Splits the given dataflow analysis partition into several partitions, one for each given checkpoint.
// The partition origA will be assigned the last checkpoint in partitionChkpts.
// If newSplit==true, this split operation creates a new split within origA's current split and place
//    the newly-generated partitions into this split. 
// If newSplit==false, the newly-generated partitions will be added to origA's current split.
// If newPartActive==true, the newly-generated partitions will be made initially active. If not,
//    they will start out in joined status.
// Returns the set of newly-created partitions.
set<IntraPartitionDataflow*> PartitionedAnalysis::split(IntraPartitionDataflow* origA, 
                                                        vector<IntraPartitionDataflowCheckpoint*> partitionChkpts,
                                                        const Function& func, NodeState* fState, 
                                                        bool newSplit, bool newPartActive)
{
/*printf("PartitionedAnalysis::split() origA=%p\n", origA);

for(vector<IntraPartitionDataflowCheckpoint*>::iterator it = partitionChkpts.begin();
    it!=partitionChkpts.end(); it++)
{ printf("    chkpt=%s\n", (*it)->str("    ").c_str()); }*/

        ROSE_ASSERT(partitionChkpts.size()>0);
        if(analysisDebugLevel>=1)
        {
                printf("@ SPLIT @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
                printf("PartitionedAnalysis::split() before: activeParts.size()=%d\n", activeParts.size());
        }

        // Only partitions that are either active or joined may call split. Joined partitions may do so during the joining process,
        // which can result in the creation of new partitions.
        ROSE_ASSERT(activeParts.find(origA) != activeParts.end() ||
                    joinParts.find(origA) != joinParts.end());
        
        if(analysisDebugLevel>=1) 
                if(origA->partitionCond != NULL)
                        cout << "origA->partitionCond = "<<origA->partitionCond->str()<<"\n";
        partSplit* split=NULL;
        if(newSplit)
                split = new partSplit(origA);
        else
                split = *(parts2splits[origA].rbegin());
        
        if(analysisDebugLevel>=1) 
                cout << "split = "<<split->str()<<", partitionChkpts.size()="<<partitionChkpts.size()<<"\n"; fflush(stdout);
        
        set<IntraPartitionDataflow*> newParts;
        
        // Generate the new analysis partitions. The last checkpoint goes to the master
        // partition and we create fresh partitions for the other checkpoints
        for(vector<IntraPartitionDataflowCheckpoint*>::iterator it = partitionChkpts.begin();
            it!=partitionChkpts.end(); )
        {
                IntraPartitionDataflowCheckpoint* chkpt = *it;
                IntraPartitionDataflow* curPartA;
                
                if(analysisDebugLevel>=1) { cout << "    chkpt->partitionCond="<<chkpt->partitionCond<<"\n"; fflush(stdout); }
                
                it++;
                
                if(it != partitionChkpts.end())
                {
                        curPartA = origA->copy();
                        split->addChild(curPartA);
                        newParts.insert(curPartA);
                        
                        parts2chkpts[curPartA] = chkpt;
                        // Create a splits list for this newly-created partition
                        // The splits list only contains the split (which may be newly-created or a copy of parts2splits[origA]) 
                        // since it will cease to exist after the next successful join of split
                        list<partSplit*> splitsList;
                        splitsList.push_back(split);
                        parts2splits[curPartA] = splitsList;
                        
                        // Set the current partition's logical condition (can't AND it because the variables involved in 
                        // the condition may have changed between the two split points)
                        curPartA->partitionCond = chkpt->partitionCond;
                        /*if(origA->partitionCond != NULL)
                                curPartA->partitionCond->andUpd(*(origA->partitionCond));*/
                        if(analysisDebugLevel>=1)
                        //{ printf("        Creating analysis partition %p (master = %p), condition = %s\n", curPartA, origA, curPartA->partitionCond->str("").c_str()); }
                        { printf("        Creating analysis partition %p (master = %p)\n", curPartA, origA); }
                                                        
                        // create a copy of the original partition's dataflow state for the new partition
                        partitionDFAnalysisState pdfas(origA, curPartA);
                        pdfas.runAnalysis(func, fState);
                        
                        // add the newly-created partition to the list of active partitions
                        if(newPartActive)
                                activeParts.insert(curPartA);
                        else
                                joinParts.insert(curPartA);
                }
                else
                {
                        //// AND this the first partition's new logical condition to the original partition's condition
                        // set the first partition's logical condition (can't AND it because the variables involved in the condition 
                        // may have changed between the two split points)
                /*printf("origA->partitionCond=%p\n", origA->partitionCond);
                printf("    partitionChkpts[0]=%s\n", partitionChkpts[0]->str("    ").c_str());*/
                        //if(parts2chkpts[origA] != NULL)
                        /*if(origA->partitionCond != NULL)
                        {
                                //parts2chkpts[origA]->partitionCond->andUpd(*(partitionChkpts[0]->partitionCond));
                                origA->partitionCond->andUpd(*(partitionChkpts[0]->partitionCond));
                                
                                if(parts2chkpts[origA])
                                        delete parts2chkpts[origA];
                                
                                parts2chkpts[origA] = partitionChkpts[0];
                                
                                // we already have a checkpoint object for the origA partition, so we can delete this new checkpoint
                                // and its internals (i.e. the partitionCond object)
                                //delete partitionChkpts[0];
                        }
                        else
                        {*/
                                origA->partitionCond = chkpt->partitionCond;
                                
                                // Delete the previous checkpoint, assuming that one exists AND we're not reusing the 
                                // old checkpoint as the new checkpoint
                                if(parts2chkpts[origA] && parts2chkpts[origA]!=chkpt)
                                        delete parts2chkpts[origA];
                                
                                parts2chkpts[origA] = chkpt;
                                
                                curPartA = origA;
                        //}
                        /*if(analysisDebugLevel>=1)
                        { printf("        Master partition %p, condition = %s\n", origA, origA->partitionCond->str("").c_str()); }*/
                }
                
                if(analysisDebugLevel>=1) 
                {
                        cout << "Updating current partition's dataflow state, parts2chkpts[curPartA]->joinNodes.size()="<<parts2chkpts[curPartA]->joinNodes.size()<<"\n";
                        cout << "    curPartA->partitionCond="<<curPartA->partitionCond<<"\n"; fflush(stdout);
                }
                // -----------------------------------------------------------------------------------------------
                // Update the current partition's current dataflow state (state at the nodes in its joinNodes set) 
                // with its new partition condition
                
                // joinNodes
/*              cout << "parts2chkpts[curPartA]->joinNodes.size()=" << parts2chkpts[curPartA]->joinNodes.size() << "\n";
                for(set<DataflowNode>::iterator itJN=parts2chkpts[curPartA]->joinNodes.begin();
                    itJN!=parts2chkpts[curPartA]->joinNodes.end(); itJN++)
                        cout << "    itJN = "<<(*itJN).getNode()->unparseToString()<<"\n";*/
                    
                for(set<DataflowNode>::iterator itJN=parts2chkpts[curPartA]->joinNodes.begin();
                    itJN!=parts2chkpts[curPartA]->joinNodes.end(); )
                {
                        DataflowNode n = *itJN;
                        
                        if(analysisDebugLevel>=1) 
                        { cout << "    joinNode "<<n.getNode()->unparseToString()<<"\n"; fflush(stdout); }
                        const vector<NodeState*> nodeStates = NodeState::getNodeStates(n);
                        //for(vector<NodeState*>::const_iterator itS = nodeStates.begin(); itS!=nodeStates.end(); )
                        vector<NodeState*>::const_iterator itS = nodeStates.begin();
                        {
                                NodeState* state = *itS;
                                Analysis* a = curPartA;
                                curPartA->initDFfromPartCond(func, n, *state, state->getLatticeBelow(a), state->getFacts(a), curPartA->partitionCond);
                        }
                        itJN++;
                }
                
                // Current Node
                if(parts2chkpts[curPartA]->curNode)
                {
                        DataflowNode n = *(parts2chkpts[curPartA]->curNode);
                        if(analysisDebugLevel>=1) cout << "    curNode "<<n.getNode()->unparseToString()<<"\n";
                        const vector<NodeState*> nodeStates = NodeState::getNodeStates(n);
                        //for(vector<NodeState*>::const_iterator itS = nodeStates.begin(); itS!=nodeStates.end(); )
                        vector<NodeState*>::const_iterator itS = nodeStates.begin();
                        {
                                NodeState* state = *itS;
                                Analysis* a = curPartA;
                                
/*ConstrGraph* cg = dynamic_cast<ConstrGraph*>(state->getLatticeBelow(a).front());
cout << "Pre-initDFfromPartCond CG="<<cg->str()<<"\n";*/
                                
                                curPartA->initDFfromPartCond(func, n, *state, state->getLatticeBelow(a), state->getFacts(a), curPartA->partitionCond);
                        }
                }
        }
        
//printf("    partitionChkpts[0]=%s\n", partitionChkpts[0]->str("    ").c_str());
//printf("    parts2chkpts[origA]=%s\n", parts2chkpts[origA]->str("    ").c_str());

        // add the new split to the original partition's list of splits
        if(newSplit)
                parts2splits[origA].push_back(split);
        
        /*for(set<IntraPartitionDataflow*>::iterator it=split->splitSet.begin(); 
                 it!=split->splitSet.end(); it++)
                printf("        partition %p, partitionCond=%p\n", *it, parts2chkpts[*it]->partitionCond);*/
        
        if(analysisDebugLevel>=1)
        {
                printf("PartitionedAnalysis::split() after: activeParts.size()=%d\n", activeParts.size());
                printf("@ SPLIT @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
        }
        
        if(analysisDebugLevel>=1) cout << "newParts.size()=="<<newParts.size()<<"\n";
        return newParts;
}

// Joins all the analysis partitions in a given split into a single partition, unioning
// their respective dataflow information
void PartitionedAnalysis::join(IntraPartitionDataflow* joinA, IntraPartitionDataflowCheckpoint* chkpt,
                               const Function& func, NodeState* fState)
{
        // Only active partitions may call join
        ROSE_ASSERT(activeParts.find(joinA) != activeParts.end());

        if(analysisDebugLevel>=1)
        { 
                printf("@ JOIN @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
                printf("        Partition %p transitioning to joining status\n", joinA);
        }
        // Move this partition from the active to joining status
        set<IntraPartitionDataflow*>::iterator it = activeParts.find(joinA);
        activeParts.erase(it);
        joinParts.insert(joinA);
        parts2chkpts[joinA] = chkpt;
        
        // The inner-most split associated with the given analysis partition
        partSplit* s = parts2splits[joinA].back();
        
        // Check to see if all the split members have already called join
        bool allJoining=true;
        for(set<IntraPartitionDataflow*>::iterator it=s->splitSet.begin(); 
            it!=s->splitSet.end(); it++)
        {
                // If the current split member has not called join
                if(joinParts.find(*it) == joinParts.end())
                {
                        if(analysisDebugLevel>=1)
                        { printf("        Split-sibling %p still not in joining status\n", *it); }
                        
                        allJoining = false;
                        break;
                }
        }
        if(analysisDebugLevel>=1) printf("allJoining=%d\n", allJoining);
        // If all the members of the split have called join
        if(allJoining)
        {
                // First check if all the joined partitions have in fact fully completed their executions
                // (a partition with a completed execution will have an empty joinNodes list)
                bool allFinished=true;
                for(set<IntraPartitionDataflow*>::iterator it=s->splitSet.begin(); 
               it!=s->splitSet.end(); it++)
           {
                ROSE_ASSERT(parts2chkpts[*it]);
                allFinished = allFinished && parts2chkpts[*it]->joinNodes.size()==0;//(parts2chkpts[*it]==NULL);
           }
           
           bool joinSplit=false;
           set<IntraPartitionDataflow*> partsToJoin;
           if(allFinished)
           {
                postFinish(s, parts2chkpts);
                joinSplit=true;
           }
           else
           {
                // Check with our derived class to see if the partitions that requested the join should truly be joined
                        partsToJoin = preJoin(s, func, fState, parts2chkpts);
                        joinSplit = (partsToJoin.size() == s->splitSet.size());
                }
                
                if(analysisDebugLevel>=1) printf("allFinished=%d, joinSplit=%d\n", allFinished, joinSplit);

                // if we're supposed to join all the split partitions back into the master partition
                if(joinSplit)
                {
                        if(analysisDebugLevel>=1)
                        { printf("        All members of split in joining status\n"); }
                        
                        // Remove the master from the set of split partitions to simplify subsequent code
                        s->splitSet.erase(s->master);
                        //ROSE_ASSERT(parts2chkpts.find(s->master) != parts2chkpts.end());
// !!! May not be true in nested partition cases because after the first inner join the master's partitionCond==NULL
// !!! which will violate this assertion during the outer split's join.
//                      ROSE_ASSERT(s->master->partitionCond!=NULL);
        
                        if(analysisDebugLevel>=1) 
                                if(s->master->partitionCond)
                                        printf("s->master->partitionCond=%p\n", s->master->partitionCond);
                        // OR their respective partition conditions of all the joining partitions, 
                        // leaving the result in the master partition's condition
                        /*for(set<IntraPartitionDataflow*>::iterator it=s->splitSet.begin(); 
                       it!=s->splitSet.end(); it++)
                        {
                                IntraPartitionDataflow* curPart = *it;
                                //ROSE_ASSERT(parts2chkpts.find(curPart) != parts2chkpts.end());
                                ROSE_ASSERT((*it)->partitionCond!=NULL);
                                
                                printf("curPart->partitionCond=%p\n",     curPart->partitionCond);
                                //parts2chkpts[s->master]->partitionCond->orUpd(*(parts2chkpts[curPart]->partitionCond));
                                s->master->partitionCond->orUpd(*(curPart->partitionCond));
                        }*/
                        // Set the master partition's condition to be NULL because
                        // the Disjunction of all the split partition conditions will refer to variables
                        // that may have been modified since the CFG node where the split occured
                        delete s->master->partitionCond;
                        s->master->partitionCond = NULL;
                        //printf("Joined partitionCond=%s\n", s->master->partitionCond->str().c_str());
                        
                        // ---------------------------------------------------------------------
                        // Union together all the dataflow states of all the analysis partitions,
                        // leaving these unions in the dataflow states associated with the master
                        // partition.
                        
                        // Convert the split set into a set of Analysis*
                        set<Analysis*> sss;
                        for(set<IntraPartitionDataflow*>::iterator it = s->splitSet.begin(); it!=s->splitSet.end(); it++)
                        { sss.insert(*it); }
                        unionDFAnalysisStatePartitions udfas(sss, s->master);
                        udfas.runAnalysis(chkpt->func, chkpt->fState);
/*UnstructuredPassInterAnalysis upia_udfas(udfas);
upia_udfas.runAnalysis();*/
                        
                        if(analysisDebugLevel>=1) cout << "Erasing non-master partitions in split\n";
                        // Delete the dataflow states of all the non-master partitions
                        deleteDFAnalysisState ddfas(s->splitSet);
                        udfas.runAnalysis(chkpt->func, chkpt->fState);
                        
                        // Delete the non-master analyses themselves, also removing them from
                        // the relevant PartitionedAnalysis data structures
                        for(set<IntraPartitionDataflow*>::iterator it=s->splitSet.begin(); 
                       it!=s->splitSet.end(); it++)
                        {
                                IntraPartitionDataflow* curPart = *it;
                                if(analysisDebugLevel>=1)
                                { printf("        Erasing partition %p\n", *it); }
                                
                                // This partition is no longer in joining status because it is being made 
                                // active (master) or is being killed (!master)
                                joinParts.erase(curPart);
                                
                                //parts2splits.erase(curPart);
                                // Pop off the inner-most split associated with the joined analysis partitions
                                parts2splits[curPart].pop_back();
                                
                                // Delete any checkpoint associated with the current partition
                                if(parts2chkpts[curPart]!=NULL)
                                        delete parts2chkpts[curPart];
                                parts2chkpts.erase(curPart);
                                
                                //delete curPart;
                        }
                        
                        // If the outer-most split has finished executing, we're done
                        if(allFinished && activeParts.size()==0)
                        {
                                // Pop off the inner-most split associated with the joined analysis partitions
                                parts2splits[s->master].pop_back();
                                
                                // Delete any checkpoint associated with the current partition
                                if(parts2chkpts[s->master]!=NULL)
                                        delete parts2chkpts[s->master];
                                parts2chkpts.erase(s->master);
                                
                                joinParts.erase(s->master);
                        }
                        // Else, if this split has not finished or this split's master is part of a higher-level
                        // unfinished split
                        else
                        {
                                // Activate the master analysis since it still needs to continue with the remainder 
                                // of the CFG
                                joinParts.erase(s->master);
                                activeParts.insert(s->master);
                        }
                        
                        // Delete the split data structure
                        delete s;
                }
                // Otherwise, release all joining threads so that they can resume execution
                else
                {
                        if(analysisDebugLevel>=1)
                        { printf("        Releasing all members of split, s->splitSet.size()=%d\n", s->splitSet.size()); }

                        // Note that this mass-release does not apply to any partitions spawned by the partitions in the split
                        for(set<IntraPartitionDataflow*>::iterator it=s->splitSet.begin(); it!=s->splitSet.end(); it++)
                   {
                        IntraPartitionDataflow* curPart = *it;
                        // If the current partition is not supposed to remained in joining status
                        if(partsToJoin.find(curPart) == partsToJoin.end())
                        {
                                // Move partition from joining to active status
                                joinParts.erase(curPart);
                                activeParts.insert(curPart);
                           }
                   }
                }
        }
        
        if(analysisDebugLevel>=1) 
                printf("@ JOIN @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
}



/**************************************
 *** unionDFAnalysisStatePartitions ***
 **************************************/
 
void unionDFAnalysisStatePartitions::visit(const Function& func, const DataflowNode& n, NodeState& state)
{
        //printf("unionDFAnalysisStatePartitions::visit() function %s() node=<%s | %s>\n", func.get_name().str(), n.getNode()->class_name().c_str(), n.getNode()->unparseToString().c_str());
        
        const vector<Lattice*>& masterLatBel = state.getLatticeBelow(master);
        //printf("    master=%p, masterLatBel.size()=%d\n", master, masterLatBel.size());
        
        state.unionLattices(unionSet, master);
}

/****************************
 *** pCFG_SplitConditions ***
 **************************** /

pCFG_SplitConditions::~pCFG_SplitConditions()
{
        // Deallocate the conditions
        for(vector<ConstrGraph*>::iterator it=conditions.begin(); it!=conditions.end(); it++)
                delete *it;
}

string pCFG_SplitConditions::str(string indent="")
{
        ostringstream outs;
        outs << indent << "[pCFG_SplitConditions : \n"; //fflush(stdout);
        outs << indent << "    type = "<<(type==dataflowSplit? "dataflowSplit" : (type==pSetSplit? "pSetSplit" : (type==noSplit? "noSplit" : "???")))<<"\n";
        outs << indent << "    conditions = \n";
        for(vector<ConstrGraph*>::iterator it=conditions.begin(); it!=conditions.end(); it++)
        { outs << indent << "        "<<(*it)->str()<<"\n"; }
}
*/


/*
 * transmap projection of annotations
 */
#include "transMap.hh"
#include "jkinclude.hh"
#include <algorithm>
#include "typeOps.hh"

/* constructor, sort mapped PSLs */
PslMapping::PslMapping(struct psl* srcPsl,
                       PslVector& mappedPsls):
    fSrcPsl(srcPsl),
    fMappedPsls(mappedPsls),
    fScore(-1) {
    if (fMappedPsls.size() > 0) {
        sortMappedPsls();
        fScore = calcPslMappingScore(srcPsl, fMappedPsls[0]); // best score due to sort
    }
}

/* free up psls */
PslMapping::~PslMapping() {
    pslFree(&fSrcPsl);
    for (size_t i = 0; i < fMappedPsls.size(); i++) {
        pslFree(&(fMappedPsls[i]));
    }
}

/* calculate the number of aligned bases */
int PslMapping::numPslAligned(const struct psl* psl) {
    return (psl->match + psl->repMatch + psl->misMatch);
}

/* Compute a mapping score between a src and mapped psl.  A perfect mapping is
 * a zero score.  Extra inserts count against the score.
 */
int PslMapping::calcPslMappingScore(const struct psl* srcPsl,
                                    const struct psl* mappedPsl) {
    return (numPslAligned(srcPsl) - numPslAligned(mappedPsl))
        + abs(int(srcPsl->qNumInsert)-int(mappedPsl->qNumInsert))
        + abs(int(srcPsl->tNumInsert)-int(mappedPsl->tNumInsert));
}

/* comparison functor based on lowest store. */
class ScoreCmp {
    public:
    struct psl* fSrcPsl;
    
    ScoreCmp(struct psl* srcPsl):
        fSrcPsl(srcPsl) {
    }

    int operator()(struct psl* mappedPsl1,
                   struct psl* mappedPsl2) const {
        return -(PslMapping::calcPslMappingScore(fSrcPsl, mappedPsl1)
                 - PslMapping::calcPslMappingScore(fSrcPsl, mappedPsl2));
    }
};


/* compare two psl to see which is better mapped. */
static struct psl* gSrcPsl = NULL;
static int mapScoreCmp(const void *va, const void *vb) {
    const struct psl *mappedPsl1 = *static_cast<const struct psl *const*>(va);
    const struct psl *mappedPsl2 = *static_cast<const struct psl *const*>(vb);
    return -(PslMapping::calcPslMappingScore(gSrcPsl, mappedPsl1)
             - PslMapping::calcPslMappingScore(gSrcPsl, mappedPsl2));
}

/* sort with best (lowest score) first */
void PslMapping::sortMappedPsls() {
    // FIXME: with this seemingly correct sort, the C++ library goes of the end of the vector and SEGVs
    // this happens on g++ 4.8.2 on Linux and 4.9 on OS/X (Mac ports).  It doesn't happen with
    // clang on OS/X.
    //sort(fMappedPsls.begin(), fMappedPsls.end(), ScoreCmp(fSrcPsl));
    gSrcPsl = fSrcPsl;
    qsort(static_cast<struct psl**>(&(fMappedPsls[0])), fMappedPsls.size(), sizeof(struct psl*), mapScoreCmp);
    gSrcPsl = NULL;
}


/* slCat that reverses parameter order, as the first list in rangeTreeAddVal
 * mergeVals function tends to be larger in degenerate cases of a huge number
 * of chains */
static void *slCatReversed(void *va, void *vb) {
    return slCat(vb, va);
}

/* add a query or target size if it doesn't already exist */
void TransMap::addSeqSize(const string& seqName,
                          int seqSize,
                          SizeMap& sizeMap) {
    if (sizeMap.find(seqName) == sizeMap.end()) {
        sizeMap[seqName] = seqSize;
    }
}

/* add a map align object to the genomeRangeTree */
void TransMap::mapAlnsAdd(struct psl *mapPsl) {
    genomeRangeTreeAddVal(fMapAlns, mapPsl->qName, mapPsl->qStart, mapPsl->qEnd, mapPsl, slCatReversed);
}

/* convert a chain to a psl, ignoring match counts, etc */
struct psl* TransMap::chainToPsl(struct chain *ch,
                                    bool swapMap) {
    int qStart = ch->qStart, qEnd = ch->qEnd;
    char strand[2] = {ch->qStrand, '\0'};
    if (ch->qStrand == '-') {
        reverseIntRange(&qStart, &qEnd, ch->qSize);
    }
    struct psl* psl = pslNew(ch->qName, ch->qSize, qStart, qEnd,
                             ch->tName, ch->tSize, ch->tStart, ch->tEnd,
                             strand, slCount(ch->blockList), 0);
    int iBlk = 0;
    for (struct cBlock *cBlk = ch->blockList; cBlk != NULL; cBlk = cBlk->next, iBlk++) {
        psl->blockSizes[iBlk] = (cBlk->tEnd - cBlk->tStart);
        psl->qStarts[iBlk] = cBlk->qStart;
        psl->tStarts[iBlk] = cBlk->tStart;
        psl->match += psl->blockSizes[iBlk];
    }
    psl->blockCount = iBlk;
    if (swapMap)
        pslSwap(psl, FALSE);
    addSeqSize(psl->qName, psl->qSize, fQuerySizes);
    addSeqSize(psl->tName, psl->tSize, fTargetSizes);
    return psl;
}

/* read a chain file, convert to mapAln object and genomeRangeTree by query locations. */
void TransMap::loadMapChains(const string& chainFile,
                             bool swapMap) {
    struct chain *ch;
    struct lineFile *chLf = lineFileOpen(toCharStr(chainFile), TRUE);
    while ((ch = chainRead(chLf)) != NULL) {
        mapAlnsAdd(chainToPsl(ch, swapMap));
        chainFree(&ch);
    }
    lineFileClose(&chLf);
}

/* constructor, loading chains */
TransMap::TransMap(const string& chainFile,
                         bool swapMap):
    fMapAlns(genomeRangeTreeNew()) {
    loadMapChains(chainFile, swapMap);
}

/* destructor */
TransMap::~TransMap() {
    struct hashCookie cookie = hashFirst(fMapAlns->jkhash);
    struct hashEl* chromEl;
    while ((chromEl = hashNext(&cookie)) != NULL) {
        for (struct range* range = genomeRangeTreeList(fMapAlns, chromEl->name);
             range != NULL; range = range->next) {
            struct psl* head = static_cast<struct psl*>(range->val);
            pslFreeList(&head);
        }
    }
    
    genomeRangeTreeFree(&fMapAlns);
}



/* map one pair of query and target PSL */
struct psl* TransMap::mapPslPair(struct psl *inPsl, struct psl *mapPsl) const {
    if (inPsl->tSize != mapPsl->qSize)
        errAbort(toCharStr("Error: inPsl %s tSize (%d) != mapping alignment %s qSize (%d) (perhaps you need to specify -swapMap?)"),
                 inPsl->tName, inPsl->tSize, mapPsl->qName, mapPsl->qSize);
    return pslTransMap(pslTransMapKeepTrans, inPsl, mapPsl);
}

/* Map a single input PSL and return a list of resulting mappings.
 * Keep PSL in the same order, even if it creates a `-' on the target. */
PslMapping* TransMap::mapPsl(struct psl* inPsl) const {
    PslVector mappedPsls;
    struct range *overMapAlnNodes = genomeRangeTreeAllOverlapping(fMapAlns, inPsl->tName, inPsl->tStart, inPsl->tEnd);
    for (struct range *overMapAlnNode = overMapAlnNodes; overMapAlnNode != NULL; overMapAlnNode = overMapAlnNode->next) {
        for (struct psl *overMapPsl = static_cast<struct psl*>(overMapAlnNode->val); overMapPsl != NULL; overMapPsl = overMapPsl->next) {
            struct psl* mappedPsl = mapPslPair(inPsl, overMapPsl);
            if (mappedPsl != NULL) {
                mappedPsls.push_back(mappedPsl);
            }
        }
    }
    return new PslMapping(inPsl, mappedPsls);
}


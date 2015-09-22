/**
 * Handling mapping of a gene object
 */
#ifndef geneMapper_hh
#define geneMapper_hh
#include "gxf.hh"
#include "featureTree.hh"
#include <map>
class TransMap;
class PslMapping;
struct psl;
class PslCursor;
class TargetAnnotations;

/* class that maps a gene to the new assemble */
class GeneMapper {
    private:
    const TransMap* fGenomeTransMap;  // genomic mapping
    const TargetAnnotations* fTargetAnnotations; // targeted genes/transcripts, maybe NULL

    typedef map<string, bool> SeqIdMap; // used for outputting GFF3 ##sequence-region metadata
    typedef pair<string, bool> SeqIdMapPair;
    SeqIdMap fMappedSeqRegionsWritten;  // mapped sequence ids that have been written

    /* check if a seqregion for seqid has been written, if so, return true,
     * otherwise record it and return false.  */
    bool checkRecordSeqRegionWritten(const string& seqid) {
        if (fMappedSeqRegionsWritten.find(seqid) == fMappedSeqRegionsWritten.end()) {
            fMappedSeqRegionsWritten.insert(SeqIdMapPair(seqid, true));
            return false;
        } else {
            return true;
        }
    }
    
    bool isSrcSeqInMapping(const GxfFeature* feature) const;
    bool isSrcSeqInMapping(FeatureNode* featureNode) const;
    void processTranscript(FeatureNode* transcriptTree,
                           ostream* transcriptPslFh) const;
    void processTranscripts(FeatureNode* geneTree,
                            ostream* transcriptPslFh) const;
    void forceToUnmapped(FeatureNode* featureNode,
                         RemapStatus remapStatus) const;
    bool haveMappedTranscripts(FeatureNode* geneTree) const;
    bool haveUnmappedTranscripts(FeatureNode* geneTree) const;
    bool hasMixedMappedSeqStrand(FeatureNode* geneTree) const;
    int calcMappedGeneLength(FeatureNode* geneTree) const;
    bool hasExcessiveSizeChange(FeatureNode* geneTree) const;

    void setNumGeneMappings(FeatureNode* geneTree) const;
    void updateMappedGeneBounds(FeatureNode* transcriptTree,
                                string& seqid, string& strand,
                                int& start, int& end) const;
    void buildMappedGeneFeature(FeatureNode* geneTree,
                                bool srcSeqInMapping) const;
    void buildUnmappedGeneFeature(FeatureNode* geneTree,
                                  bool srcSeqInMapping) const;
    void buildGeneFeature(FeatureNode* geneTree) const;
    void outputMappedSeqRegionIfNeed(FeatureNode* geneTree,
                                     ostream& mappedGxfFh);
    void outputMapped(FeatureNode* featureNode,
                      ostream& mappedGxfFh) const;
    void outputUnmapped(FeatureNode* featureNode,
                        ostream& unmappedGxfFh) const;
    void outputFeatures(FeatureNode* geneNode,
                        ostream& mappedGxfFh,
                        ostream& unmappedGxfFh);
    void outputInfoHeader(ostream& mappingInfoFh) const;
    const GxfFeature* getTargetAnnotation(FeatureNode* featureNode) const;
    const string& getTargetAnnotationStatus(FeatureNode* featureNode,
                                            const GxfFeature* targetFeature) const;
    const string& getTargetAnnotationBiotype(FeatureNode* featureNode,
                                             const GxfFeature* targetFeature) const;
    void outputFeatureInfo(FeatureNode* featureNode,
                           ostream& mappingInfoFh) const;
    void outputInfo(FeatureNode* geneNode,
                    ostream& mappingInfoFh) const;
    void processGene(GxfParser *gxfParser,
                     GxfFeature* geneFeature,
                     ostream& mappedGxfFh,
                     ostream& unmappedGxfFh,
                     ostream& mappingInfoFh,
                     ostream* transcriptPslFh);
    void processRecord(GxfParser *gxfParser,
                       GxfRecord* gxfRecord,
                       ostream& mappedGxfFh,
                       ostream& unmappedGxfFh,
                       ostream& mappingInfoFh,
                       ostream* transcriptPslFh);
    public:
    /* Constructor */
    GeneMapper(const TransMap* genomeTransMap,
               const TargetAnnotations* targetAnnotations):
        fGenomeTransMap(genomeTransMap),
        fTargetAnnotations(targetAnnotations) {
    }

    /* Map a GFF3/GTF */
    void mapGxf(GxfParser *gxfParser,
                ostream& mappedGxfFh,
                ostream& unmappedGxfFh,
                ostream& mappingInfoFh,
                ostream* transcriptPslFh);
};

#endif

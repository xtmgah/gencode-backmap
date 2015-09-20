/*
 * Extremely naive and specialized GFF3 and GTF parsers.
 *
 * The goal preserve the exact structure of the GFF3/GTF files, including
 * comments, and only update coordinates (and occasionally split lines).
 * Neither parser in the kent tree design for this.
 *
 * These parser assume the ordering of the GENCODE GFF3/GTF files.
 */

#ifndef gxf_hh
#define gxf_hh
#include "typeOps.hh"
#include <queue>
#include <stdexcept>
#include <algorithm>
using namespace std;

/* it seems so stupid to need to keep writing one-off GFF/GTF parsers */

class GxfFeature;
class FIOStream;

typedef enum {
    GXF_UNKNOWN_FORMAT,
    GFF3_FORMAT,
    GTF_FORMAT,
} GxfFormat;

/* get a base id, deleting the version, if it exists */
static inline string getBaseId(const string& id) {
    size_t idot = id.find_last_of('.');
    if (idot == string::npos) {
        return id;
    } else {
        return id.substr(0, idot);
    }
}

/*
 * GxF base record type.  Use instanceOf to determine actually type
 */
class GxfRecord {
    public:
    /* destructor */
    virtual ~GxfRecord() {
    }

    /* return record as a string */
    virtual string toString() const = 0;
};
typedef vector<GxfRecord*> GxfRecordVector;

/*
 * non-feature line.
 */
class GxfLine: public string, public GxfRecord {
    public:
    GxfLine(const string& line):
        string(line) {
    }

    /* destructor */
    virtual ~GxfLine() {
    }

    /* return record as a string */
    virtual string toString() const {
        return *this;
    }
};

/* attribute/value pair */
class AttrVal {
    public:
    const string fName;
    const string fVal;
    const bool fQuoted;

    AttrVal(const string& name, const string& val, bool quoted=false):
        fName(name), fVal(val), fQuoted(quoted) {
        if (stringEmpty(fName)) {
            throw invalid_argument("empty attribute name");
        }
        if (stringEmpty(fVal)) {
            throw invalid_argument("empty attribute value");
        }
    }

    /* copy constructor */
    AttrVal(const AttrVal& src):
        fName(src.fName), fVal(src.fVal), fQuoted(src.fQuoted) {
    }
};

/* list of attributes, derived class of specific types are create for GFF3 and GTF */
class AttrVals: public vector<const AttrVal*> {
    // n.b.  this keeps pointers rather than values due to reallocation if vector changes
    public:
    /* empty constructor */
    AttrVals() {
    }

    /* copy constructor */
    AttrVals(const AttrVals& src) {
        for (size_t i = 0; i < src.size(); i++) {
            push_back(new AttrVal(*(src[i])));
        }
    }
    
    /* destructor */
    ~AttrVals() {
        for (size_t i = 0; i < size(); i++) {
            delete (*this)[i];
        }
    }

    /* does the attribute exist */
    bool exists(const string& name) const {
        return findIdx(name) >= 0;
    }
    
    /* find the index of an attribute or -1 if not found */
    int findIdx(const string& name) const {
        for (int i = 0; i < size(); i++) {
            if ((*this)[i]->fName == name) {
                return i;
            }
        }
        return -1;
    }

    
    /* get a attribute, NULL if it doesn't exist */
    const AttrVal* find(const string& name) const {
        int i = findIdx(name);
        if (i < 0) {
            return NULL;
        } else {
            return (*this)[i];
        }
    }
    
    /* get a attribute, error it doesn't exist */
    const AttrVal* get(const string& name) const {
        const AttrVal* attrVal = find(name);
        if (attrVal == NULL) {
            throw invalid_argument("Attribute not found: " + name);
        }
        return attrVal;
    }

    /* add an attribute */
    void add(const AttrVal& attrVal) {
        push_back(new AttrVal(attrVal));
    }

    /* add or replace an attribute */
    void update(const AttrVal& attrVal) {
        int idx = findIdx(attrVal.fName);
        if (idx < 0) {
            add(attrVal);
        } else {
            delete (*this)[idx];
            (*this)[idx] = new AttrVal(attrVal);
        }
    }
};

/*
 * A row parsed from a GTF/GFF file. Immutable object.
 */
class GxfFeature: public GxfRecord {
public:
    // Standard feature names
    static const string GENE;
    static const string TRANSCRIPT;
    static const string EXON;
    static const string CDS;
    static const string START_CODON;
    static const string UTR;
    static const string STOP_CODON;
    static const string STOP_CODON_REDEFINED_AS_SELENOCYSTEINE;

    // standard attribute names
    static const string ID_ATTR;
    static const string PARENT_ATTR;
    static const string GENE_ID_ATTR;
    static const string TRANSCRIPT_ID_ATTR;
    static const string GENE_TYPE_ID_ATTR;
    static const string TRANSCRIPT_TYPE_ID_ATTR;
    static const string EXON_ID_ATTR;
    
    // columns parsed from file.
    const string fSeqid;
    const string fSource;
    const string fType;
    const int fStart;
    const int fEnd;
    const string fScore;
    const string fStrand;
    const string fPhase;
    AttrVals fAttrs;     // attribute maybe modified

    protected:
    string baseColumnsAsString() const;
    
    public:
    /* construct a new feature object */
    GxfFeature(const string& seqid, const string& source, const string& type,
               int start, int end, const string& score, const string& strand,
               const string& phase, const AttrVals& attrs):
        fSeqid(seqid), fSource(source), fType(type),
        fStart(start), fEnd(end),
        fScore(score), fStrand(strand),
        fPhase(phase), fAttrs(attrs) {
        assert(strand.size() == 1);
        assert(phase.size() == 1);
    }

    /* clone the feature */
    virtual GxfFeature* clone() const = 0;
    
    /* get the format */
    virtual GxfFormat getFormat() const = 0;
    
    /* destructor */
    virtual ~GxfFeature() {
    }

    /* get all attribute */
    const AttrVals& getAttrs() const {
        return fAttrs;
    }

    /* get all attribute */
    AttrVals& getAttrs() {
        return fAttrs;
    }
    
    /* does the attribute exist */
    bool hasAttr(const string& name) const {
        return fAttrs.exists(name);
    }

    /* get a attribute, NULL if it doesn't exist */
    const AttrVal* findAttr(const string& name) const {
        return fAttrs.find(name);
    }

    /* get a attribute, error it doesn't exist */
    const AttrVal* getAttr(const string& name) const {
        return fAttrs.get(name);
    }

    /* get a attribute value, error it doesn't exist */
    const string& getAttrValue(const string& name) const {
        return getAttr(name)->fVal;
    }

    /* get a attribute value, default it doesn't exist */
    const string& getAttrValue(const string& name, 
                               const string& defaultVal) const {
        const AttrVal* attrVal = findAttr(name);
        if (attrVal == NULL) {
            return defaultVal;
        } else {
            return attrVal->fVal;
        }
    }

    /* get the id based on feature type, or empty string if it doesn't have an
     * id */
    const string& getTypeId() const;
    
    /* get the biotype based on feature type, or empty string if it doesn't have an
     * id */
    const string& getTypeBiotype() const;
    
    /* get the size of the feature */
    int size() const {
        return (fEnd - fStart)+1;
    }

    /* does this feature overlap another */
    bool overlaps(const GxfFeature* other) const {
        if ((fSeqid != other->fSeqid) || (fStrand != other->fStrand)) {
            return false;
        } else if ((fStart > other->fEnd) || (fEnd < other->fStart)) {
            return false;
        } else {
            return true;
        }
    }
};

/* create the appropriate feature type */
GxfFeature* gxfFeatureFactory(GxfFormat gxfFormat,
                              const StringVector& columns);

/* create the appropriate feature type */
GxfFeature* gxfFeatureFactory(GxfFormat gxfFormat,
                              const string& seqid, const string& source, const string& type,
                              int start, int end, const string& score, const string& strand,
                              const string& phase, const AttrVals& attrs);

/* vector of feature objects, doesn't own features */
class GxfFeatureVector: public vector<GxfFeature*> {
    public:
    /* free all features in the vector */
    void free() {
        for (int i = 0; i < size(); i++) {
            delete (*this)[i];
        }
        clear();
    }

    /* do we contain a particular feature object? */
    bool contains(const GxfFeature* feature) const {
        return std::find(begin(), end(), feature) != end();
    }
    
    /* sort the vector */
    void sort() {
        std::sort(begin(), end(),
                  [](const GxfFeature* a, const GxfFeature* b) -> bool {
                      if (a->fStrand == "+") {
                          return a->fStart > b->fStart;
                      } else {
                          return a->fStart < b->fStart;
                      }   
                  });
    }
};

/**
 * gff3 or gtf parser.
 */
class GxfParser {
    private:
    FIOStream* fIn;  // input stream
    GxfFormat fGxfFormat; // format of file
    queue<GxfRecord*> fPending; // FIFO of pushed records to be read before file

    StringVector splitFeatureLine(const string& line) const;
    GxfRecord* read();
    
    public:
    /* constructor that opens file, which maybe compressed.
    * if gxfFormat is unknown, guess from filename*/
    GxfParser(const string& fileName,
              GxfFormat gxfFormat=GXF_UNKNOWN_FORMAT);

    /* Get format from file name, or error */
    static GxfFormat formatFromFileName(const string& fileName);

    /* destructor */
    ~GxfParser();

    
    /* Read the next record, either queued by push() or from the file , use
     * instanceOf to determine the type.  Return NULL on EOF.
     */
    GxfRecord* next();

    /* Return a record to be read before the file. */
    void push(GxfRecord* gxfRecord) {
        fPending.push(gxfRecord);
    }

    /* Accessors */
    GxfFormat getGxfFormat() const {
        return fGxfFormat;
    }
};

#endif

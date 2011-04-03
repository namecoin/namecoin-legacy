class CNameDB : public CDB
{
protected:
    bool fHaveParent;
public:
    CNameDB(const char* pszMode="r+") : CDB("nameindex.dat", pszMode) {
        fHaveParent = false;
    }

    CNameDB(const char* pszMode, CDB& parent) : CDB("nameindex.dat", pszMode) {
        vTxn.push_back(parent.GetTxn());
        fHaveParent = true;
    }

    ~CNameDB()
    {
        if (fHaveParent)
            vTxn.erase(vTxn.begin());
    }

    bool WriteName(vector<unsigned char>& name, vector<CDiskTxPos> vtxPos)
    {
        return Write(make_pair(string("namei"), name), vtxPos);
    }

    bool ReadName(vector<unsigned char>& name, vector<CDiskTxPos>& vtxPos)
    {
        return Read(make_pair(string("namei"), name), vtxPos);
    }

    bool ExistsName(vector<unsigned char>& name)
    {
        return Exists(make_pair(string("namei"), name));
    }

    bool EraseName(vector<unsigned char>& name)
    {
        return Erase(make_pair(string("namei"), name));
    }

    bool ScanNames(
            const vector<unsigned char>& vchName,
            int nMax,
            vector<pair<vector<unsigned char>, CDiskTxPos> >& nameScan);

    bool test();
}
;

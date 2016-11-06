#ifndef ORG_WALRUS_ALARMFUCK_PHLIST_H
#define ORG_WALRUS_ALARMFUCK_PHLIST_H

#include <unordered_set>
#include <unordered_map>
#include <gtkmm/treestore.h>
extern "C" {
#include <sys/types.h>
}

class AlarmFuckFileChooser;

class PathHashEntry{
private:
    Gtk::TreeStore::iterator iter;
public:
    PathHashEntry(){};
    PathHashEntry(std::unordered_set<std::string>* sp, off_t ts) : subfilesPointer(sp), totalSize(ts){}
    std::unordered_set<std::string>* subfilesPointer;
    off_t totalSize;
};

class PathList
{
protected:
    off_t totalSize;
public:
    bool empty(){return llPathMap.empty();};
    off_t get_size(){return totalSize;};
    PathList& operator=(const PathList&);
    std::unordered_map<std::string, PathHashEntry> llPathMap;
};

class PathListMirror
{
public:
    PathListMirror(){};
    virtual ~PathListMirror();
    virtual void notify(const std::string& message){};
    virtual void insert_entry(const Glib::ustring&, off_t, const std::string&){};
};

// an unordered map for file keeping purposes. Keys are pathname strings and
// values are a struct of an unordered set pointer and an off_t. For paths
// representing files the pointer is null and the off_t is the file size in
// bytes. For directories, the pointer points to a list of all subfiles and the
// off_t is their total size.  If a file from an already included directory is
// added, it is ignored. If a directory containing already included files is
// added, those files' entries are deleted.
class PathHashList : public PathList
{
private:
    struct stat *statBuf;
    PathHashEntry* currentTopEntry;
public:
    PathHashList()
    {
	totalSize = 0;
	statBuf = new struct stat;
	currentObject = this;
    };
    bool check_and_add_path(const std::string&, PathListMirror&);
    bool check_and_add_path(const std::string& message){
	PathListMirror dummy;
	return check_and_add_path(message, dummy);};
    void remove_path(std::string);
    bool import_file(const std::string&);
    void populate_path_hash_view(std::vector<std::string>);

    static int traversal_func(const char *, const struct stat *, int);
    static PathHashList* currentObject;
};


#endif /* end of include guard: ORG_WALRUS_ALARMFUCK_PHLIST_H */

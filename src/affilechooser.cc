#include "affilechooser.h"
#include "aflaunch.h"
#include "common.h"
#include <gtkmm/filechooserdialog.h>
#include <iostream>
#include <cstdlib>
extern "C" {
#include <ftw.h>
}

const char FILE_DELIM = '/';

AlarmFuckFileChooser::AlarmFuckFileChooser(AlarmFuckLauncher& parent) :
    addFilesButton("Add Files"),
    addFolderButton("Add Folder"),
    removeButton("Remove"),
    importButton("Import"),
    doneButton("Done"),
    cancelButton("Cancel"),
    bottomButtonHBox(Gtk::ORIENTATION_HORIZONTAL),
    vBox(Gtk::ORIENTATION_VERTICAL, 5),
    parentWindow(parent)
{
    /* UI initialization {{{ */
    set_border_width(10);
    set_title("Choose Files to Scramble");
    set_default_size(500, 400);
    set_transient_for(parentWindow);
    
    // Widget setup
    bottomButtonHBox.set_layout(Gtk::BUTTONBOX_SPREAD);
    bottomButtonHBox.set_spacing(40);
    bottomButtonHBox.add(addFilesButton);
    bottomButtonHBox.add(addFolderButton);
    bottomButtonHBox.add(removeButton);
    bottomButtonHBox.add(importButton);
    bottomButtonHBox.add(doneButton);
    bottomButtonHBox.add(cancelButton);
    vBox.pack_start(fileScroll, Gtk::PACK_EXPAND_WIDGET);
    vBox.pack_start(infoScroll, Gtk::PACK_SHRINK);
    vBox.pack_start(bottomButtonHBox, Gtk::PACK_SHRINK);
    add(vBox);
    
    fileScroll.add(fileView);
    fileScroll.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    infoScroll.add(infoTextView);
    infoScroll.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    infoScroll.set_size_request(-1,80);
    
    addFilesButton.signal_clicked().connect(sigc::bind<std::string>(sigc::mem_fun(*this,
		&AlarmFuckFileChooser::on_add_button_clicked), "File"));
    addFolderButton.signal_clicked().connect(sigc::bind<std::string>(sigc::mem_fun(*this,
		&AlarmFuckFileChooser::on_add_button_clicked), "Folder"));
    removeButton.signal_clicked().connect(sigc::mem_fun(*this,
		&AlarmFuckFileChooser::on_remove_button_clicked));
    cancelButton.signal_clicked().connect(sigc::mem_fun(*this,
		&AlarmFuckFileChooser::on_cancel_button_clicked));
    importButton.signal_clicked().connect(sigc::mem_fun(*this,
		&AlarmFuckFileChooser::on_import_button_clicked));
    doneButton.signal_clicked().connect(sigc::mem_fun(*this,
		&AlarmFuckFileChooser::on_done_button_clicked));
    
    // fileView
    filenameTreeStore = Gtk::ListStore::create(fileViewColumnRecord);
    fileView.set_model(filenameTreeStore);
    fileView.append_column("Type", fileViewColumnRecord.typeCol);
    fileView.append_column("Path", fileViewColumnRecord.nameCol);
    (fileView.get_selection())->set_mode(Gtk::SELECTION_MULTIPLE);
    //fileView.set_rubber_banding(true);
    
    // textBuffer
    infoTextBuffer = infoTextView.get_buffer();
    /* }}} UI initialization */

    show_all_children();
    infoScroll.hide();

}

void AlarmFuckFileChooser::on_add_button_clicked(const std::string& typeStr)
{
    Gtk::FileChooserAction fcAction = (typeStr == "File") ? Gtk::FILE_CHOOSER_ACTION_OPEN : Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER;
    Gtk::FileChooserDialog dialog("Choose " + typeStr + "s to Add", fcAction);
    dialog.set_transient_for(*this);
    dialog.set_select_multiple(true);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Open", Gtk::RESPONSE_OK);
    if(dialog.run() != Gtk::RESPONSE_OK) return;
    // Now add selected files to our TreeView
    populate_path_hash_view(dialog.get_filenames());
}

void AlarmFuckFileChooser::populate_path_hash_view(std::vector<std::string> fileList){
    int valid = 0, invalid = 0;
    for(auto it = fileList.begin(); it != fileList.end(); it++){
	if(check_and_add_path(*it)) valid++;
	else invalid++;
    }
    notify(std::to_string(valid) + " paths added, " + std::to_string(invalid) + " skipped.\n");
}

void AlarmFuckFileChooser::check_path_prerequisites(const std::string& filePath){
    std::string prefixString = "Skipped " + filePath + ": ";
    // check absoluteness and that it's not the whole system
    if(filePath.front() != FILE_DELIM){
	throw AfUserException("not an absolute path");
    }
    if(filePath == "/"){
	//TODO: also check whether parent directories of the alarmfuck executables are included in path
	throw AfUserException("please do not gamble away the whole system");
    }

    // check if the path is included already
    if( hashMap.count(filePath) ){
	throw AfUserException("already listed");
    }
}

std::pair<bool, off_t> AlarmFuckFileChooser::is_accessible_directory(const std::string& filePath){
    int permissionMask;
    bool isDir = false;
    std::unique_ptr<struct stat> statBuf{new struct stat};
    if(stat(filePath.c_str(), statBuf.get()) != 0)
	throw AfSystemException("Error processing " + filePath);
    switch(statBuf->st_mode & S_IFMT)
    {
	// ordinary file
	case S_IFREG:
	    permissionMask = W_OK;
	    break;

	    // folder
	case S_IFDIR:
	    permissionMask = W_OK | X_OK;
	    isDir = true;
	    break;

	default:
	    throw AfUserException("incompatible file type");
    }

    if( access(filePath.c_str(), permissionMask) != 0 ){
	throw AfUserException("no write permission.");
    }
    return std::pair<bool,size_t>(isDir, statBuf->st_size);
}


bool AlarmFuckFileChooser::check_and_add_path(const std::string& filePath)
{
    bool isDir = false;
    off_t size = 0;
    try{
	check_path_prerequisites(filePath);
	auto resPair = is_accessible_directory(filePath);
	isDir = resPair.first;
	size = resPair.second;
    } catch (AfUserException& exc) {
	notify("Skipped " + filePath + ": " + exc.what() + ".\n");
	return false;
    } catch (AfSystemException& exc) {
	notify(exc.get_message() + ": " + exc.what() + ".\n");
	return false;
    }

    // update hash lists and fileView
    if( !isDir ) insert_checked_entry(filePath, size);
    else {// directory ... do the file tree walk
	//currentTopEntry = new PathHashEntry(new std::unordered_set<std::string>, 0);
	int (AlarmFuckFileChooser::*myFunction)(const char *, const struct stat *, int)  = &AlarmFuckFileChooser::traversal_func;
	ftw( filePath.c_str(), myFunction, 15 );

	std::pair<std::string,PathHashEntry> somePair(filePath, *currentTopEntry);
	llPathMap.insert(somePair);
    }
    return true;
}

int AlarmFuckFileChooser::traversal_func(const char *fpath, const struct stat *sb, int nopen){
    // first check permissions
    switch(sb->st_mode & S_IFMT)
    {
	// ordinary file
	case S_IFREG:
	    if( access(fpath, W_OK) ) return 0;
	    break;
	    // folder
	case S_IFDIR:
	    if( access(fpath, W_OK | X_OK) ) return 0;
	    break;

	default:
	    return 0;

    }
    // check if subpath is present already (and delete it if it is)
    // TODO: refine
    if(hashMap.count(fpath)){
	hashMap.erase(std::string(fpath));
    }
    // add it
    if((sb->st_mode & S_IFMT) == S_IFREG){
	//currentObject->currentTopEntry->subfilesPointer->insert(fpath);
	//currentObject->currentTopEntry->totalSize += sb->st_size;
	//currentObject->totalSize += sb->st_size;
    }
    return 0;
}

void AlarmFuckFileChooser::insert_checked_entry(const std::string& filePath, off_t size){
    Gtk::TreeModel::iterator row = filenameTreeStore->append();
    (*row)[fileViewColumnRecord.sizeCol] = size;
    (*row)[fileViewColumnRecord.nameCol] = filePath;
    // TODO: what is the validity of these iterators later
    std::pair<std::string,Gtk::TreeStore::iterator> somePair{filePath, row};
    hashMap.insert(somePair);
    totalSize += size;
}

void AlarmFuckFileChooser::erase(Gtk::TreeIter& blob){
    filenameTreeStore->erase(blob);
}

void AlarmFuckFileChooser::on_remove_button_clicked()
{
    std::vector<Gtk::TreeModel::Path> pathList = (fileView.get_selection())->get_selected_rows();
    Gtk::TreeModel::iterator iter;
    // Didn't find any documentation that reverse deletion is safe, but seems to work
    for(auto it = pathList.rbegin(); it != pathList.rend(); it++){
	iter = filenameTreeStore->get_iter(*it);
	pathHashList->remove_path((*iter)[fileViewColumnRecord.nameCol]);
    }
}

void AlarmFuckFileChooser::on_import_button_clicked()
{
    Gtk::FileChooserDialog dialog("Import from File", Gtk::FILE_CHOOSER_ACTION_OPEN);
    dialog.set_transient_for(*this);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Open", Gtk::RESPONSE_OK);
    if(dialog.run() != Gtk::RESPONSE_OK) return;
    // TODO: test!
    std::cout << "Importing file: " << dialog.get_filename() << std::endl;
    pathHashList->import_file(dialog.get_filename());
}

void AlarmFuckFileChooser::on_done_button_clicked()
{
    parentWindow.update_user_hash_list();
    parentWindow.signal_updated_files();
    parentWindow.check_good_to_go();
    hide();
}

void AlarmFuckFileChooser::notify(const std::string& message){
    infoScroll.show();
    infoTextBuffer->insert(infoTextBuffer->end(), message);
}

Gtk::TreeModel::iterator AlarmFuckFileChooser::insert_entry(const Glib::ustring& type, off_t size, const std::string& name){
    Gtk::TreeModel::iterator row = filenameTreeStore->append();
    (*row)[fileViewColumnRecord.typeCol] = type;
    (*row)[fileViewColumnRecord.nameCol] = name;
    return row;
}

#include "affilechooser.h"
#include "aflaunch.h"
#include "common.h"
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/cellrenderertext.h>
#include <iostream>
#include <cstdlib>

const char FILE_DELIM = '/';

AlarmFsckFileChooser::AlarmFsckFileChooser(AlarmFsckLauncher& parent) :
    addFilesButton("Add Files"),
    addFolderButton("Add Folder"),
    removeButton("Remove"),
    importButton("Import"),
    doneButton("Done"),
    cancelButton("Cancel"),
    bottomButtonHBox(Gtk::ORIENTATION_HORIZONTAL),
    vBox(Gtk::ORIENTATION_VERTICAL, 5),
    totalSize(0),
    parentWindow{parent}
{
    get_set_first_obj_ptr(this);

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
    // TODO: make this autoscroll when new text appears
    
    addFilesButton.signal_clicked().connect(sigc::bind<std::string>(sigc::mem_fun(*this,
		&AlarmFsckFileChooser::on_add_button_clicked), "File"));
    addFolderButton.signal_clicked().connect(sigc::bind<std::string>(sigc::mem_fun(*this,
		&AlarmFsckFileChooser::on_add_button_clicked), "Folder"));
    removeButton.signal_clicked().connect(sigc::mem_fun(*this,
		&AlarmFsckFileChooser::on_remove_button_clicked));
    cancelButton.signal_clicked().connect(sigc::mem_fun(*this,
		&AlarmFsckFileChooser::on_cancel_button_clicked));
    importButton.signal_clicked().connect(sigc::mem_fun(*this,
		&AlarmFsckFileChooser::on_import_button_clicked));
    doneButton.signal_clicked().connect(sigc::mem_fun(*this,
		&AlarmFsckFileChooser::on_done_button_clicked));
    
    // fileView, 
    filenameTreeStore = Gtk::TreeStore::create(fileViewColumnRecord);
    fileView.set_model(filenameTreeStore);

    // set up columns and their pretty printing
    int nCols = fileView.append_column("Path", fileViewColumnRecord.nameCol);
    Gtk::TreeViewColumn* column = fileView.get_column(nCols - 1);
    column->set_cell_data_func(*(column->get_first_cell()),
	    sigc::mem_fun(*this, &AlarmFsckFileChooser::render_short_subpaths));

    nCols = fileView.append_column("Size", fileViewColumnRecord.sizeCol);
    (fileView.get_selection())->set_mode(Gtk::SELECTION_MULTIPLE);
    column = fileView.get_column(nCols - 1);
    column->set_cell_data_func(*(column->get_first_cell()),
	    sigc::mem_fun(*this, &AlarmFsckFileChooser::render_humanized_cumulative_byte_count));
    
    // textBuffer
    infoTextBuffer = infoTextView.get_buffer();
    /* }}} UI initialization */

    show_all_children();
    infoScroll.hide();

}

void AlarmFsckFileChooser::render_humanized_cumulative_byte_count(Gtk::CellRenderer* render, const Gtk::TreeModel::iterator& iter){
    if(Gtk::CellRendererText* textRend = dynamic_cast<Gtk::CellRendererText*>(render)){
	if((*iter)[fileViewColumnRecord.isDir])
	    textRend->property_text() = "+ " + afCommon::humanize_byte_count(get_cumulative_size(iter));
	else
	    textRend->property_text() = afCommon::humanize_byte_count((*iter)[fileViewColumnRecord.sizeCol]);
    }
}

off_t AlarmFsckFileChooser::get_cumulative_size(const Gtk::TreeModel::iterator& iter){
    if((*iter)[fileViewColumnRecord.hasCumSize])
	return (*iter)[fileViewColumnRecord.sizeCol];
    off_t cumSize = 0;
    for(auto& child : iter->children())
	cumSize += get_cumulative_size(child);
    (*iter)[fileViewColumnRecord.hasCumSize] = true;
    (*iter)[fileViewColumnRecord.sizeCol] = cumSize;
    return cumSize;
}

void AlarmFsckFileChooser::render_short_subpaths(Gtk::CellRenderer* render, const Gtk::TreeModel::iterator& iter){
    // only display the file/directory names for all but the top-level paths, which remain absolute
    if(Gtk::CellRendererText* textRend = dynamic_cast<Gtk::CellRendererText*>(render)){
	std::string filePath = (*iter)[fileViewColumnRecord.nameCol];
	textRend->property_text() = (filenameTreeStore->iter_depth(iter) == 0) ?
	    filePath : filePath.substr(filePath.rfind(file_delim) + 1);
    }
}

void AlarmFsckFileChooser::on_add_button_clicked(const std::string& typeStr)
{
    Gtk::FileChooserAction fcAction = (typeStr == "File") ? Gtk::FILE_CHOOSER_ACTION_OPEN : Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER;
    Gtk::FileChooserDialog dialog("Choose " + typeStr + "s to Add", fcAction);
    dialog.set_transient_for(*this);
    dialog.set_select_multiple(true);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Open", Gtk::RESPONSE_OK);
    if(dialog.run() != Gtk::RESPONSE_OK) return;
    // Now add selected files to our TreeView
    populate_file_view(dialog.get_filenames());
}

void AlarmFsckFileChooser::populate_file_view(std::vector<std::string> fileList){
    int valid = 0, invalid = 0;
    for(auto it = fileList.begin(); it != fileList.end(); it++){
	if(check_and_add_path(*it)) valid++;
	else invalid++;
    }
    notify(std::to_string(valid) + " paths added, " + std::to_string(invalid) + " skipped.\n");
    if(valid > 0) set_updated(true);
}

void AlarmFsckFileChooser::on_remove_button_clicked()
{
    // this may only be called on top-level entries
    std::vector<Gtk::TreeModel::Path> treePathList = (fileView.get_selection())->get_selected_rows();
    Gtk::TreeModel::iterator iter;
    std::string currFilePath;
    int count = 0;
    for(auto treePath = treePathList.begin(); treePath != treePathList.end(); treePath++){
	iter = filenameTreeStore->get_iter(*treePath);
	currFilePath = (*iter)[fileViewColumnRecord.nameCol];
	if(filenameTreeStore->iter_depth(iter) != 0){
	    notify("Skipping " + currFilePath + ": only top-level entries can be erased\n");
	    continue;
	}
	// if the path has previously been removed
	if(hashMap.count(currFilePath) == 0) continue;
	// otherwise
	erase_subtree(iter);
	filenameTreeStore->erase(iter);
	count++;
	notify("Removed " + currFilePath +"\n");
    }
    notify("Erased " + std::to_string(count) + " top-level path" + ((count == 1)? "" : "s") + ".\n");
    if(count > 0) set_updated(true);
}

void AlarmFsckFileChooser::on_import_button_clicked()
{
    Gtk::FileChooserDialog dialog("Import from File", Gtk::FILE_CHOOSER_ACTION_OPEN);
    dialog.set_transient_for(*this);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Open", Gtk::RESPONSE_OK);
    if(dialog.run() != Gtk::RESPONSE_OK) return;
    std::cout << "Importing file: " << dialog.get_filename() << std::endl;
    try {
	import_file(dialog.get_filename());
    } catch (AfUserException& exc) {
	notify(exc.what());
    } catch (AfSystemException& exc) {
	notify(exc.get_message() + ": " + exc.what() + ".\n");
    }
}

void AlarmFsckFileChooser::on_done_button_clicked()
{
    parentWindow.check_good_to_go();
    hide();
}

void AlarmFsckFileChooser::notify(const std::string& message){
    infoScroll.show();
    infoTextBuffer->insert(infoTextBuffer->end(), message);
}

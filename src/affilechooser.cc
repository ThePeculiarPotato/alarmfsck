#include "affilechooser.h"
#include <gtkmm/filechooserdialog.h>
#include <iostream>
#include <cstdlib>

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
    fileViewListStore = Gtk::ListStore::create(fileViewColumnRecord);
    fileView.set_model(fileViewListStore);
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

void AlarmFuckFileChooser::set_hash_list(PathHashList& phl){
    pathHashList = &phl;
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
    pathHashList->populate_path_hash_view(dialog.get_filenames());
}

void AlarmFuckFileChooser::erase(Gtk::TreeIter& blob){
    fileViewListStore->erase(blob);
}

void AlarmFuckFileChooser::on_remove_button_clicked()
{
    std::vector<Gtk::TreeModel::Path> pathList = (fileView.get_selection())->get_selected_rows();
    Gtk::TreeModel::iterator iter;
    // Didn't find any documentation that reverse deletion is safe, but seems to work
    for(auto it = pathList.rbegin(); it != pathList.rend(); it++){
	iter = fileViewListStore->get_iter(*it);
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
    Gtk::TreeModel::iterator row = fileViewListStore->append();
    (*row)[fileViewColumnRecord.typeCol] = type;
    (*row)[fileViewColumnRecord.nameCol] = name;
    return row;
}

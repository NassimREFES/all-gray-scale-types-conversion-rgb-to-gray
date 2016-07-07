#include <wx/wx.h>
#include <wx/scrolwin.h>
#include <wx/notebook.h>
#include <wx/dc.h>
#include <wx/dcmemory.h>
#include <wx/dcbuffer.h>
#include <wx/infobar.h>

#include <iostream>
#include <vector>
#include <algorithm>
#include <exception>

using namespace std;

struct rgb_color
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
	
	rgb_color() : rgb_color(0, 0, 0) { } // dark default color
	rgb_color(unsigned char rr, unsigned char gg, unsigned char bb)
		: r(rr), g(gg), b(bb) { }
		
	const rgb_color& get() const { return *this; }
};

struct conversion
{
	enum methods { 
		AVERAGE, 
		// original recommandation ITU-R
		LUMINANCE_BT_709,
		LUMINANCE_BT_601,
		// converting an RGB triplet to an HSL triplet
		DESATURATION,
		// artistic effect
		DECOMPOSITION_MAX, // brighter grayscale
		DECOMPOSITION_MIN,  // darker grayscale
		FULL_RED, // only red channel
		FULL_GREEN, // only green channel
		FULL_BLUE, // only blue channel
		// custom of gray shades
		SHADES_4, // ( 4 color : black, dark gray, light gray, and white )
		SHADES_16,
		SHADES_32,
		SHADES_64
	};
	
	static rgb_color convert(rgb_color pixel, methods m = AVERAGE)
	{
		unsigned char gray;
		switch (m) {
			case AVERAGE :
				gray = (pixel.r + pixel.g + pixel.b) / 3;
				break;
			case LUMINANCE_BT_709 :
				gray = 0.2126*pixel.r + 0.7152*pixel.g + 0.0722*pixel.b;
				break;
			case LUMINANCE_BT_601 :
				gray = 0.299*pixel.r + 0.587*pixel.g + 0.114*pixel.b;
				break;
			case DESATURATION :
				gray = (max(pixel.r, max(pixel.g, pixel.b))+min(pixel.r, min(pixel.g, pixel.b)))/2;
				break;
			case DECOMPOSITION_MAX :
				gray = max(pixel.r, max(pixel.g, pixel.b));
				break;
			case DECOMPOSITION_MIN :
				gray = min(pixel.r, min(pixel.g, pixel.b));
				break;
			case FULL_RED   : gray = pixel.r; break;
			case FULL_GREEN : gray = pixel.g; break;
			case FULL_BLUE  : gray = pixel.b; break;
			case SHADES_4    : return shades_of_gray_convert(pixel, 4);
			case SHADES_16   : return shades_of_gray_convert(pixel, 16);
			case SHADES_32   : return shades_of_gray_convert(pixel, 32);
			case SHADES_64   : return shades_of_gray_convert(pixel, 64);
			default : throw runtime_error("not existe");
		}
		return rgb_color(gray, gray, gray);
	}
	
	/*
		number of shades is between 2-256
		2 : black, white
		4 : black, dark gray, light gray, and white
		...
	*/
	static rgb_color shades_of_gray_convert(rgb_color pixel, unsigned char numberOfShade)
	{
		unsigned char gray;
		unsigned char conversionFactor = 255 / (numberOfShade - 1);
		unsigned char averageValue = (pixel.r + pixel.g + pixel.b) / 3;
		gray = (((double)averageValue / conversionFactor) + 0.5)*conversionFactor;
		return rgb_color(gray, gray, gray);
	}
	
	static vector<rgb_color> convert(vector<rgb_color> pixels, methods m = AVERAGE)
	{
		vector<rgb_color> v_gray;
		for (rgb_color pixel : pixels) 
			v_gray.push_back(convert(pixel, m));
		return v_gray;
	}	
};

vector< vector< rgb_color > > pixels_of_image(wxBitmap bmp)
{
	wxImage image_origin = bmp.ConvertToImage();
	vector< vector< rgb_color > > pixels_origin_image;
	vector< rgb_color > row_pixels;
	for (unsigned int i = 0; i < image_origin.GetWidth(); ++i) {
		for (unsigned int j = 0; j < image_origin.GetHeight(); ++j) {
			rgb_color pixel(image_origin.GetRed(i, j), image_origin.GetGreen(i, j), image_origin.GetBlue(i, j));
			row_pixels.push_back(pixel);
		}
		pixels_origin_image.push_back(row_pixels);
		row_pixels.clear();
	}
	
	return pixels_origin_image;
}

enum {
	ID_IMAGE_LINK = 100,
	ID_IMAGE_CHOICE,
	ID_RUN_ALGORITHM,
	ID_ALGORITHM_CHOICE,
	ID_COMPARE_TWO_IMAGE
};

class display_image : public wxScrolledWindow
{
public :
	display_image(wxNotebook* parent, wxString s = "")
		: wxScrolledWindow(parent, wxID_ANY),
		  m_bmp(s, wxBITMAP_TYPE_JPEG)
	{	
		if (!s.IsEmpty())
			SetScrollbars(1, 1, m_bmp.GetWidth(), m_bmp.GetHeight(), 0, 0);
		
		Connect(wxEVT_PAINT, wxPaintEventHandler(display_image::OnPaint));
	}	
	
	wxBitmap insert_image(const wxString& s)
	{
		wxBitmap bmp(s, wxBITMAP_TYPE_JPEG);
		m_bmp = bmp;
		SetScrollbars(1, 1, m_bmp.GetWidth(), m_bmp.GetHeight(), 0, 0);
		Refresh();
		return m_bmp;
	}
	
	void insert_image(wxBitmap bmp)
	{
		m_bmp = bmp;
		SetScrollbars(1, 1, m_bmp.GetWidth(), m_bmp.GetHeight(), 0, 0);
		Refresh();
	}
	
	wxBitmap insert_image(wxBitmap bmp, conversion::methods m)
	// insert and applie one method conversion
	{
		vector< vector< rgb_color > > poi = pixels_of_image(bmp);
		wxImage img(bmp.ConvertToImage());
		// convert to gray
		for (unsigned int i = 0; i < poi.size(); ++i)
			poi[i] = conversion::convert(poi[i], m);
		
		// set image pixels		
		for (unsigned int i = 0; i < poi.size(); ++i)
				for (unsigned int j = 0; j < poi[i].size(); ++j)
					img.SetRGB(i, j, poi[i][j].r, poi[i][j].g,
						poi[i][j].b);
		
		m_bmp = wxBitmap(img);
		SetScrollbars(1, 1, poi.size(), poi[0].size(), 0, 0);
		Refresh();
		return m_bmp;
	}
	
	void OnPaint(wxPaintEvent& WXUNUSED(event))
	{
		wxPaintDC dc(this);
		
		int width  = GetClientSize().x;
		int height = GetClientSize().y;
		wxBitmap cur = m_bmp.GetSubBitmap(wxRect(GetScrollPos(wxSB_HORIZONTAL), 
			GetScrollPos(wxSB_VERTICAL), width, height));
		dc.DrawBitmap(cur, 0, 0);

		dc.SetPen(wxNullPen);
	}
	
private :
	wxBitmap m_bmp;
};

class GrayScale : public wxFrame
{
public :
	GrayScale();
	void OnPaint(wxPaintEvent& event);	
	void OnImageChoice(wxCommandEvent& event);
	void OnRunAlgorithm(wxCommandEvent& event);
	void OnCompareTwoAlgorithm(wxCommandEvent& event);
	
private :
	wxNotebook* left;
	wxNotebook* right;
	display_image* original_image;
	display_image* converted_image;
	
	wxTextCtrl* m_image_link;
	wxButton*   m_image_choice;
	wxString m_image_choice_link;
	wxButton*   m_run_algorithm;
	wxChoice*   m_algorithm_choice;
	
	wxButton* m_compare_two_image;
	wxChoice* m_gray1;
	wxChoice* m_gray2;
};

GrayScale::GrayScale()
	: wxFrame(NULL, wxID_ANY, wxT("GrayScaleConverter"), wxDefaultPosition, wxSize(900, 500))	
{
	left = new wxNotebook(this, wxID_ANY);
	original_image = new display_image(left, wxT("00.jpg"));
	left->AddPage(original_image, "Original Image");
	
	right = new wxNotebook(this, wxID_ANY);
	converted_image = new display_image(right, wxT("00c.jpg"));
	right->AddPage(converted_image, "Converted Image");
	
	// -----------------------------------------------------------
	
	m_image_choice_link = wxT("/home/neys/Bureau/00c.jpg");
	m_image_link = new wxTextCtrl(this, ID_IMAGE_LINK, m_image_choice_link);
	m_image_link->SetEditable(false);
	m_image_choice = new wxButton(this, ID_IMAGE_CHOICE, wxT("IMAGE CHOICE"));
	m_run_algorithm = new wxButton(this, ID_RUN_ALGORITHM, wxT("RUN"));
	
	wxString choice[13] = { "AVERAGE", "LUMINANCE_BT_709", "LUMINANCE_BT_601", 
		"DESATURATION", "DECOMPOSITION_MAX", "DECOMPOSITION_MIN", "FULL_RED",
		"FULL_GREEN", "FULL_BLUE", "SHADES_4", "SHADES_16", "SHADES_32",
		"SHADES_64" };
	m_algorithm_choice = new wxChoice(this, ID_ALGORITHM_CHOICE, wxDefaultPosition,
		wxDefaultSize, 13, choice);
	m_algorithm_choice->SetSelection(0);
	
	m_compare_two_image = new wxButton(this, ID_COMPARE_TWO_IMAGE, wxT("COMPARE TWO GRAY METHODS..."));
	wxStaticText* g1 = new wxStaticText(this, wxID_ANY, wxT("GRAY #1"));
	wxStaticText* g2 = new wxStaticText(this, wxID_ANY, wxT("GRAY #2"));
	m_gray1 = new wxChoice(this, wxID_ANY, wxDefaultPosition,
		wxDefaultSize, 13, choice);	
	m_gray1->SetSelection(0);
	m_gray2 = new wxChoice(this, wxID_ANY, wxDefaultPosition,
		wxDefaultSize, 13, choice);	
	m_gray2->SetSelection(1);
	
	wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* hbox2 = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* hbox3 = new wxBoxSizer(wxHORIZONTAL);
	
	hbox2->Add(m_image_link, 1, wxEXPAND | wxALL, 5);
	hbox2->Add(m_image_choice, 0, wxEXPAND | wxALL, 5);
	hbox2->Add(m_run_algorithm, 0, wxEXPAND | wxALL, 5);
	hbox2->Add(m_algorithm_choice, 0, wxEXPAND | wxALL, 5);
	
	hbox3->Add(g1, 0, wxTOP, 10);
	hbox3->Add(m_gray1, 0, wxALL | wxEXPAND, 5);
	hbox3->Add(g2, 0, wxTOP, 10);
	hbox3->Add(m_gray2, 0, wxALL | wxEXPAND, 5);
	hbox3->Add(m_compare_two_image, 1, wxALL | wxEXPAND, 5);
	
	wxBoxSizer* hbox = new wxBoxSizer(wxHORIZONTAL);
	
	hbox->Add(left, 1, wxEXPAND | wxALL, 10);
	hbox->Add(right, 1, wxEXPAND | wxALL, 10);
	
	vbox->Add(hbox, 1, wxEXPAND | wxALL, 10);
	vbox->Add(hbox2, 0, wxEXPAND | wxALL, 10);
	vbox->Add(hbox3, 0, wxEXPAND | wxALL, 10);
	
	Connect(wxEVT_PAINT, wxPaintEventHandler(GrayScale::OnPaint));
	Connect(ID_IMAGE_CHOICE, wxEVT_BUTTON, wxCommandEventHandler(GrayScale::OnImageChoice));
	Connect(ID_RUN_ALGORITHM, wxEVT_BUTTON, wxCommandEventHandler(GrayScale::OnRunAlgorithm));
	Connect(ID_COMPARE_TWO_IMAGE, wxEVT_BUTTON, wxCommandEventHandler(GrayScale::OnCompareTwoAlgorithm));
	SetSizer(vbox);
	Centre();
}

void GrayScale::OnCompareTwoAlgorithm(wxCommandEvent& event)
{
	wxBitmap bmp(m_image_choice_link, wxBITMAP_TYPE_JPEG);
	
	wxImage g1 = converted_image->insert_image(bmp, 
		(conversion::methods)m_gray1->GetSelection()).ConvertToImage();
	wxImage g2 = converted_image->insert_image(bmp, 
		(conversion::methods)m_gray2->GetSelection()).ConvertToImage();
		
	for (int i = 0; i < g1.GetWidth(); ++i) {
		for (int j = 0; j < g1.GetHeight(); ++j) {
			if (g1.GetRed(i, j) == g2.GetRed(i, j) || 
				g1.GetGreen(i, j) == g2.GetGreen(i, j) ||
				g1.GetBlue(i, j) == g2.GetBlue(i, j) ) 
					g1.SetRGB(i, j, 255, 0, 0);
		}
	}
	
	converted_image->insert_image(wxBitmap(g1));
	wxMessageBox("equivalent pixels in the two images involves a red pixel", "Information", wxICON_INFORMATION );
}

void GrayScale::OnImageChoice(wxCommandEvent& event)
{
	wxFileDialog openFileDialog(this, wxT(""), "", "",
		"Image file (*.jpg)|*.jpg", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
	if (openFileDialog.ShowModal() == wxID_OK) {
		m_image_choice_link = openFileDialog.GetPath();
		m_image_link->SetValue(m_image_choice_link);
		original_image->insert_image(m_image_choice_link);
	}	
}

void GrayScale::OnRunAlgorithm(wxCommandEvent& event)
{
	wxBitmap bmp(m_image_choice_link, wxBITMAP_TYPE_JPEG);
	
	converted_image->insert_image(bmp, 
		(conversion::methods)m_algorithm_choice->GetSelection());
}

void GrayScale::OnPaint(wxPaintEvent& event)
{
	
}

class app : public wxApp
{
public :
	virtual bool OnInit();	
};

IMPLEMENT_APP(app)

bool app::OnInit()
{	
	wxInitAllImageHandlers();
	GrayScale* gs = new GrayScale();
	gs->Show(true);
	return true;
}


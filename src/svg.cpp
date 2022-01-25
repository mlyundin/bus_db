#include "svg.h"

#include <algorithm>
#include <iterator>

namespace Svg {
    std::ostream& operator<<(std::ostream& out, Svg::Point p) {
        return out << p.x << ',' << p.y;
    }

    std::ostream& operator<<(std::ostream& out, const Svg::Rgb& rgb) {
        return out<<"rgb(" << rgb.red << ',' << rgb.green << ',' << rgb.blue << ')';
    }

    std::ostream& operator<<(std::ostream& out, const Svg::Rgba& rgba) {
        return out<<"rgba(" << rgba.red << ',' << rgba.green << ',' << rgba.blue << ',' << rgba.alpha << ')';
    }

    std::ostream& operator<<(std::ostream& out, std::monostate) {
        return out << "none";
    }

    std::ostream& operator<<(std::ostream& out, const Svg::Color& color) {
        std::visit([&out](auto&& value){out << value;}, color);
        return out;
    }

    template<class T>
    std::ostream& operator<<(std::ostream& out, const BaseObject<T>& object) {
        out << "fill=\"" << object.fill_color_;
        out << "\" ";
        out << "stroke=\"" << object.stroke_color_;
        out << "\" ";
        out << "stroke-width=\"" << object.stroke_width_ << "\" ";
        if (object.stroke_line_cap_) {
            out << "stroke-linecap=\"" << *object.stroke_line_cap_ << "\" ";
        }
        if (object.stroke_line_join_) {
            out << "stroke-linejoin=\"" << *object.stroke_line_join_ << "\" ";
        }
        return out;
    }

    std::ostream& operator<<(std::ostream& out, const Circle& circle) {
        out << "<circle ";
        out << "cx=\"" << circle.center_.x << "\" ";
        out << "cy=\"" << circle.center_.y << "\" ";
        out << "r=\"" << circle.radius_ << "\" ";

        return out << static_cast<const BaseObject<Circle>&>(circle) << "/>";
    }

    Circle& Circle::SetCenter(Point center){center_ = center; return *this;}
    Circle& Circle::SetRadius(double radius) {radius_ = radius; return *this;}

    std::ostream& operator<<(std::ostream& out, const Polyline& poly) {
        out << "<polyline ";
        out << "points=\"";
        std::copy(poly.points_.begin(), poly.points_.end(), std::ostream_iterator<Point>(out, " "));
        out << "\" ";

        return out << static_cast<const BaseObject<Polyline>&>(poly) << "/>";
    }

    Polyline& Polyline::AddPoint(Point p) {points_.push_back(p);return *this;}

    std::ostream& operator<<(std::ostream& out, const Text& text) {
        out << "<text ";
        out << "x=\"" << text.point_.x << "\" ";
        out << "y=\"" << text.point_.y << "\" ";
        out << "dx=\"" << text.offset_.x << "\" ";
        out << "dy=\"" << text.offset_.y << "\" ";
        out << "font-size=\"" << text.font_size_ << "\" ";
        if (text.font_family_) {
            out << "font-family=\"" << *text.font_family_ << "\" ";
        }
        if (text.font_weight_) {
            out << "font-weight=\"" << *text.font_weight_ << "\" ";
        }
        out << static_cast<const BaseObject<Text>&>(text);
        out << ">";
        out << text.data_;
        return out << "</text>";
    }

    Text& Text::SetPoint(Point p) {point_ = p; return *this;}
    Text& Text::SetOffset(Point p) {offset_ = p; return *this;}
    Text& Text::SetFontSize(uint32_t size) {font_size_ = size; return *this;}
    Text& Text::SetFontFamily(const std::string& fontFamily) {font_family_ = fontFamily; return *this;}
    Text& Text::SetFontWeight(const std::string& fontWeight){font_weight_ = fontWeight; return  *this;}
    Text& Text::SetData(const std::string& data) {data_ = data; return *this;}

    std::ostream& operator<<(std::ostream& out, const Document& doc) {
        out << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>";
        out << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">";
        const auto add_to_out = [&out](auto&& arg){out << arg;};
        for (const auto& item: doc.items_) {
            std::visit(add_to_out, item);
        }
        return out<<"</svg>";
    }

    void Document::Render(std::ostream& out) const {
        out << *this;
    }
}
#pragma once

#include <iostream>
#include <variant>
#include <optional>
#include <vector>
#include <string>

namespace Svg {
    struct Point {
        double x = 0, y = 0;
    };

    struct Rgb {
        int red = 0, green = 0, blue = 0;
    };

    struct Rgba: public Rgb {
        double alpha = {};
    };

    using Color = std::variant<std::monostate, std::string, Rgb, Rgba>;
    const Color NoneColor{};

    template<typename T>
    class BaseObject; // forward declare to make function declaration possible

    template<typename T> // declaration
    std::ostream& operator<<(std::ostream&, const BaseObject<T>&);

    template <class T>
    class BaseObject {
    protected:
        Color fill_color_;
        Color stroke_color_;
        double stroke_width_ = 1.0;
        std::optional<std::string> stroke_line_cap_;
        std::optional<std::string> stroke_line_join_;

    public:
        T& SetFillColor(const Color& color) {fill_color_ = color; return static_cast<T&>(*this);}
        T& SetStrokeColor(const Color& color) {stroke_color_ = color; return static_cast<T&>(*this);}
        T& SetStrokeWidth(double width) { stroke_width_ = width; return static_cast<T&>(*this);}
        T& SetStrokeLineCap(const std::string& lineCap) {stroke_line_cap_ = lineCap; return static_cast<T&>(*this);}
        T& SetStrokeLineJoin(const std::string& lineJoin) {stroke_line_join_ = lineJoin; return static_cast<T&>(*this);}

        friend std::ostream& operator<< <> (std::ostream&, const BaseObject&);
    };

    class Circle: public BaseObject<Circle> {
        Point center_;
        double radius_ = 1;
    public:
        Circle& SetCenter(Point);
        Circle& SetRadius(double);

        friend std::ostream& operator<<(std::ostream&, const Circle&);
    };

    class Polyline: public BaseObject<Polyline> {
        std::vector<Point> points_;
    public:
        Polyline& AddPoint(Point);
        friend std::ostream& operator<<(std::ostream&, const Polyline&);
    };

    class Text: public BaseObject<Text> {
        Point point_;
        Point offset_;
        uint32_t font_size_ = 1;
        std::optional<std::string> font_family_;
        std::optional<std::string> font_weight_;
        std::string data_;
    public:
        Text& SetPoint(Point);
        Text& SetOffset(Point);
        Text& SetFontSize(uint32_t);
        Text& SetFontFamily(const std::string&);
        Text& SetFontWeight(const std::string&);
        Text& SetData(const std::string&);
        friend std::ostream& operator<<(std::ostream&, const Text&);
    };
    class Document {
        using Item = std::variant<Text, Polyline, Circle>;
        std::vector<Item> items_;

    public:
        template<class T>
        Document& Add(T&& item) {
            items_.push_back({std::forward<T>(item)});
            return *this;
        }
        void Render(std::ostream& out) const;
        friend std::ostream& operator<<(std::ostream&, const Document&);
    };
}

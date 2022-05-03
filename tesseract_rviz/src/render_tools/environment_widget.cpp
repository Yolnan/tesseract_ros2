#include <tesseract_common/macros.h>
TESSERACT_COMMON_IGNORE_WARNINGS_PUSH
#include <rviz_common/display_context.hpp>
#include <rviz_common/properties/bool_property.hpp>
#include <rviz_common/properties/color_property.hpp>
#include <rviz_common/properties/float_property.hpp>
#include <rviz_common/properties/property.hpp>
#include <rviz_common/properties/ros_topic_property.hpp>
#include <rviz_common/properties/string_property.hpp>
#include <rviz_common/properties/enum_property.hpp>
#include <rviz_common/window_manager_interface.hpp>
TESSERACT_COMMON_IGNORE_WARNINGS_POP

#include <tesseract_rosutils/utils.h>

#include <tesseract_environment/environment.h>

#include <tesseract_rviz/render_tools/visualization_widget.h>
#include <tesseract_rviz/render_tools/link_widget.h>
#include <tesseract_rviz/render_tools/environment_widget.h>

namespace tesseract_rviz
{
const std::string DEFAULT_GET_ENVIRONMENT_CHANGES_SERVICE = "get_tesseract_changes_rviz";
const std::string DEFAULT_MODIFY_ENVIRONMENT_SERVICE = "modify_tesseract_rviz";

/**
 * @brief Declare URDF and SRDF parameters for later accessing
 */
void declareDescriptionParameters(rclcpp::Node::SharedPtr, const std::string& desc_param);

EnvironmentWidget::EnvironmentWidget(rviz_common::properties::Property* widget, rviz_common::Display* display, const std::string& widget_ns)
  : widget_(widget)
  , display_(display)
  , visualization_(nullptr)
  , env_(nullptr)
  , update_required_(false)
  , update_state_(true)
  , load_tesseract_(true)
{
  environment_widget_counter_++;
  if (widget_ns.empty())
  {
    environment_widget_id_ = environment_widget_counter_;
    widget_ns_ = "env_widget_" + std::to_string(environment_widget_id_);
  }
  else
  {
    widget_ns_ = widget_ns;
  }

  main_property_ = new rviz_common::properties::Property("Environment", "", "Tesseract Environment", widget_, nullptr, this);

  display_mode_property_ = new rviz_common::properties::EnumProperty("Display Mode",
                                                                     "URDF",
                                                                     "Leverage URDF or connect to monitor namespace",
                                                                     main_property_,
                                                                     SLOT(changedDisplayMode()),
                                                                     this);

  display_mode_property_->addOptionStd("URDF", 0);
  display_mode_property_->addOptionStd("Monitor", 1);

  environment_namespace_property_ = new rviz_common::properties::StringProperty("Interface Namespace",
                                                             QString::fromUtf8(widget_ns_.c_str()),
                                                             "The namespace used for the service interface associated "
                                                             "with this rviz plugin environment monitor",
                                                             main_property_);
  environment_namespace_property_->setReadOnly(true);

  display_mode_string_property_ = new rviz_common::properties::StringProperty("Monitor Namespace",
                                                                              "tesseract_environment",
                                                                              "This will monitor this namespace for changes",
                                                                              main_property_,
                                                                              SLOT(changedDisplayModeString()),
                                                                              this);

  root_link_name_property_ = new rviz_common::properties::StringProperty("Root Link",
                                                      "",
                                                      "Shows the name of the root link for the urdf",
                                                      main_property_,
                                                      SLOT(changedRootLinkName()),
                                                      this);
  root_link_name_property_->setReadOnly(true);

  alpha_property_ = new rviz_common::properties::FloatProperty("Alpha",
                                            1.0f,
                                            "Specifies the alpha for the links with geometry",
                                            main_property_,
                                            SLOT(changedURDFSceneAlpha()),
                                            this);
  alpha_property_->setMin(0.0);
  alpha_property_->setMax(1.0);

  urdf_description_property_ = new rviz_common::properties::StringProperty("URDF Description",
                                                        "robot_description",
                                                        "The name of the ROS parameter where the URDF for the robot is "
                                                        "loaded",
                                                        main_property_,
                                                        SLOT(changedURDFDescription()),
                                                        this);

  enable_link_highlight_ = new rviz_common::properties::BoolProperty("Show Highlights",
                                                  true,
                                                  "Specifies whether link highlighting is enabled",
                                                  main_property_,
                                                  SLOT(changedEnableLinkHighlight()),
                                                  this);
  enable_visual_visible_ = new rviz_common::properties::BoolProperty("Show Visual",
                                                  true,
                                                  "Whether to display the visual representation of the environment.",
                                                  main_property_,
                                                  SLOT(changedEnableVisualVisible()),
                                                  this);

  enable_collision_visible_ = new rviz_common::properties::BoolProperty("Show Collision",
                                                     false,
                                                     "Whether to display the collision "
                                                     "representation of the environment.",
                                                     main_property_,
                                                     SLOT(changedEnableCollisionVisible()),
                                                     this);

  show_all_links_ = new rviz_common::properties::BoolProperty(
      "Show All Links", true, "Toggle all links visibility on or off.", main_property_, SLOT(changedAllLinks()), this);
}

EnvironmentWidget::~EnvironmentWidget() = default;

void EnvironmentWidget::onInitialize(VisualizationWidget::Ptr visualization,
                                     tesseract_environment::Environment::Ptr env,
                                     rviz_common::DisplayContext* context,
                                     rclcpp::Node::SharedPtr update_node,
                                     bool update_state)
{
  visualization_ = std::move(visualization);
  env_ = std::move(env);
  node_ = update_node;
  update_state_ = update_state;

  if (!visualization_->isInitialized())
    visualization_->initialize(true, true, true, true);

/*
  using std::placeholders::_1;
  using std::placeholders::_2;
  using std::placeholders::_3;

  widget_ns_ = environment_namespace_property_->getStdString();
  modify_environment_server_ = node_->create_service<tesseract_msgs::srv::ModifyEnvironment>(
      widget_ns_ + DEFAULT_MODIFY_ENVIRONMENT_SERVICE, std::bind(&EnvironmentWidget::modifyEnvironmentCallback, this, _1, _2, _3));

  get_environment_changes_server_ = node_->create_service<tesseract_msgs::srv::GetEnvironmentChanges>(
      widget_ns_ + DEFAULT_GET_ENVIRONMENT_CHANGES_SERVICE, std::bind(&EnvironmentWidget::getEnvironmentChangesCallback, this, _1, _2, _3));
*/
  changedEnableVisualVisible();
  changedEnableCollisionVisible();
  changedDisplayMode();

  if (display_mode_property_->getOptionInt() == 0) // URDF from parameter
  {
    declareDescriptionParameters(node_, urdf_description_property_->getString().toStdString());
  }
}

void EnvironmentWidget::onEnable()
{
  load_tesseract_ = true;  // allow loading of robot model in update()
  if (visualization_)
    visualization_->setVisible(true);
}

void EnvironmentWidget::onDisable()
{
  // monitor_->stopMonitoringEnvironment();
  if (visualization_)
    visualization_->setVisible(false);
}

void EnvironmentWidget::onUpdate()
{
  if (load_tesseract_)
  {
    loadEnvironment();
    update_required_ = true;
  }

  std::shared_lock<std::shared_mutex> lock;
  if (monitor_ != nullptr)
    lock = monitor_->lockEnvironmentRead();

  if (visualization_ && env_->isInitialized())
  {
    int monitor_revision = env_->getRevision();
    if (monitor_revision > revision_)
    {
      tesseract_environment::Commands commands = env_->getCommandHistory();
      for (std::size_t i = static_cast<std::size_t>(revision_); i < static_cast<std::size_t>(monitor_revision); ++i)
        applyEnvironmentCommands(*commands[i]);

      revision_ = monitor_revision;
      visualization_->update();
    }
  }

  if (visualization_ && env_->isInitialized())
  {
    if (update_required_ || state_timestamp_ != env_->getCurrentStateTimestamp())
    {
      update_required_ = false;
      visualization_->update(env_->getState().link_transforms);
      state_timestamp_ = env_->getCurrentStateTimestamp();
    }
  }
}

void EnvironmentWidget::onReset() { load_tesseract_ = true; }

void EnvironmentWidget::changedAllLinks()
{
  rviz_common::properties::Property* links_prop = widget_->subProp("Links");
  QVariant value(show_all_links_->getBool());

  for (int i = 0; i < links_prop->numChildren(); ++i)
  {
    rviz_common::properties::Property* link_prop = links_prop->childAt(i);
    link_prop->setValue(value);
  }
}

// void EnvironmentStateDisplay::setHighlightedLink(const std::string& link_name, const std_msgs::ColorRGBA& color)
//{
//  RobotLink* link = state_->getRobot().getLink(link_name);
//  if (link)
//  {
//    link->setColor(color.r, color.g, color.b);
//    link->setRobotAlpha(color.a * alpha_property_->getFloat());
//  }
//}

// void EnvironmentStateDisplay::unsetHighlightedLink(const std::string& link_name)
//{
//  RobotLink* link = state_->getRobot().getLink(link_name);
//  if (link)
//  {
//    link->unsetColor();
//    link->setRobotAlpha(alpha_property_->getFloat());
//  }
//}

void EnvironmentWidget::changedEnableLinkHighlight()
{
  //  if (enable_link_highlight_->getBool())
  //  {
  //    for (std::map<std::string, std_msgs::ColorRGBA>::iterator it = highlights_.begin(); it != highlights_.end();
  //    ++it)
  //    {
  //      setHighlightedLink(it->first, it->second);
  //    }
  //  }
  //  else
  //  {
  //    for (std::map<std::string, std_msgs::ColorRGBA>::iterator it = highlights_.begin(); it != highlights_.end();
  //    ++it)
  //    {
  //      unsetHighlightedLink(it->first);
  //    }
  //  }
}

void EnvironmentWidget::changedEnableVisualVisible()
{
  visualization_->setVisualVisible(enable_visual_visible_->getBool());
}

void EnvironmentWidget::changedEnableCollisionVisible()
{
  visualization_->setCollisionVisible(enable_collision_visible_->getBool());
}

//static bool operator!=(const std_msgs::msg::ColorRGBA& a, const std_msgs::msg::ColorRGBA& b)
//{
//  return a.r != b.r || a.g != b.g || a.b != b.b || a.a != b.a;
//}

void EnvironmentWidget::changedRootLinkName() {}

void EnvironmentWidget::changedDisplayMode()
{
  if (display_mode_property_->getOptionInt() == 0)
  {
    display_mode_string_property_->setName("URDF Parameter");
    display_mode_string_property_->setDescription("The URDF parameter to use for creating the environment within RViz");
    display_mode_string_property_->setValue("robot_description");
  }
  else if (display_mode_property_->getOptionInt() == 1)
  {
    display_mode_string_property_->setName("Monitor Namespace");
    display_mode_string_property_->setDescription("This will monitor this namespace for changes.");
    display_mode_string_property_->setValue("tesseract_environment");
  }

  if (display_->isEnabled())
    onReset();
}

void EnvironmentWidget::changedDisplayModeString()
{
  /*
  using std::placeholders::_1;
  using std::placeholders::_2;
  using std::placeholders::_3;

  // Need to kill services and relaunch with new name
  modify_environment_server_.reset();
  get_environment_changes_server_.reset();

  widget_ns_ = environment_namespace_property_->getStdString();
  modify_environment_server_ = node_->create_service<tesseract_msgs::srv::ModifyEnvironment>(
      widget_ns_ + DEFAULT_MODIFY_ENVIRONMENT_SERVICE, std::bind(&EnvironmentWidget::modifyEnvironmentCallback, this, _1, _2, _3));

  get_environment_changes_server_ = node_->create_service<tesseract_msgs::srv::GetEnvironmentChanges>(
      widget_ns_ + DEFAULT_GET_ENVIRONMENT_CHANGES_SERVICE, std::bind(&EnvironmentWidget::getEnvironmentChangesCallback, this, _1, _2, _3));
  */

  if (display_->isEnabled())
    onReset();
}

void EnvironmentWidget::changedURDFDescription()
{
  if (display_->isEnabled())
    onReset();

  declareDescriptionParameters(node_, urdf_description_property_->getString().toStdString());
}
void EnvironmentWidget::changedURDFSceneAlpha()
{
  if (visualization_)
  {
    visualization_->setAlpha(alpha_property_->getFloat());
    update_required_ = true;
  }
}

bool EnvironmentWidget::applyEnvironmentCommands(const tesseract_environment::Command& command)
{
  update_required_ = true;
  switch (command.getType())
  {
    case tesseract_environment::CommandType::ADD_LINK:
    {
      const auto& cmd = static_cast<const tesseract_environment::AddLinkCommand&>(command);

      bool link_exists = false;
      bool joint_exists = false;
      std::string link_name, joint_name;

      if (cmd.getLink() != nullptr)
      {
        link_name = cmd.getLink()->getName();
        link_exists = visualization_->getLink(link_name) != nullptr;
      }

      if (cmd.getJoint() != nullptr)
      {
        joint_name = cmd.getJoint()->getName();
        joint_exists = visualization_->getJoint(joint_name) != nullptr;
      }

      // These check are handled by the environment but just as a precaution adding asserts here
      assert(!(link_exists && !cmd.replaceAllowed()));
      assert(!(joint_exists && !cmd.replaceAllowed()));
      assert(!(link_exists && cmd.getJoint() && !joint_exists));
      assert(!(!link_exists && joint_exists));

      if (link_exists && joint_exists)
      {
        if (!visualization_->addLink(cmd.getLink()->clone(), true) ||
            !visualization_->addJoint(cmd.getJoint()->clone(), true))
          return false;
      }
      else if (link_exists && cmd.replaceAllowed())
      {
        if (!visualization_->addLink(cmd.getLink()->clone(), true))
          return false;
      }
      else if (!link_exists)
      {
        if (!visualization_->addLink(cmd.getLink()->clone()) || !visualization_->addJoint(cmd.getJoint()->clone()))
          return false;
      }
      else
      {
        return false;
      }

      break;
    }
    case tesseract_environment::CommandType::ADD_SCENE_GRAPH:
    {
      // Done
      const auto& cmd = static_cast<const tesseract_environment::AddSceneGraphCommand&>(command);

      if (cmd.getJoint() == nullptr)
      {
        if (!visualization_->addSceneGraph(*(cmd.getSceneGraph()), cmd.getPrefix()))
          return false;
      }
      else
      {
        if (!visualization_->addSceneGraph(*(cmd.getSceneGraph()), cmd.getJoint()->clone(), cmd.getPrefix()))
          return false;
      }

      break;
    }
    case tesseract_environment::CommandType::MOVE_LINK:
    {
      const auto& cmd = static_cast<const tesseract_environment::MoveLinkCommand&>(command);
      if (!visualization_->moveLink(cmd.getJoint()->clone()))
        return false;

      break;
    }
    case tesseract_environment::CommandType::MOVE_JOINT:
    {
      // Done
      const auto& cmd = static_cast<const tesseract_environment::MoveJointCommand&>(command);
      if (!visualization_->moveJoint(cmd.getJointName(), cmd.getParentLink()))
        return false;

      break;
    }
    case tesseract_environment::CommandType::REMOVE_LINK:
    {
      // Done
      const auto& cmd = static_cast<const tesseract_environment::RemoveLinkCommand&>(command);
      if (!visualization_->removeLink(cmd.getLinkName()))
        return false;

      break;
    }
    case tesseract_environment::CommandType::REMOVE_JOINT:
    {
      // Done
      const auto& cmd = static_cast<const tesseract_environment::RemoveJointCommand&>(command);
      if (!visualization_->removeJoint(cmd.getJointName()))
        return false;

      break;
    }
    case tesseract_environment::CommandType::REPLACE_JOINT:
    {
      // Done
      const auto& cmd = static_cast<const tesseract_environment::ReplaceJointCommand&>(command);
      if (!visualization_->addJoint(cmd.getJoint()->clone(), true))
        return false;

      break;
    }
    case tesseract_environment::CommandType::CHANGE_LINK_ORIGIN:
    {
      assert(false);
      break;
    }
    case tesseract_environment::CommandType::CHANGE_JOINT_ORIGIN:
    {
      // Done
      const auto& cmd = static_cast<const tesseract_environment::ChangeJointOriginCommand&>(command);
      if (!visualization_->changeJointOrigin(cmd.getJointName(), cmd.getOrigin()))
        return false;

      break;
    }
    case tesseract_environment::CommandType::CHANGE_LINK_COLLISION_ENABLED:
    {
      // Done
      const auto& cmd = static_cast<const tesseract_environment::ChangeLinkCollisionEnabledCommand&>(command);
      visualization_->setLinkCollisionEnabled(cmd.getLinkName(), cmd.getEnabled());
      break;
    }
    case tesseract_environment::CommandType::CHANGE_LINK_VISIBILITY:
    {
      // Done
      const auto& cmd = static_cast<const tesseract_environment::ChangeLinkVisibilityCommand&>(command);
      visualization_->setLinkCollisionEnabled(cmd.getLinkName(), cmd.getEnabled());
      break;
    }
    case tesseract_environment::CommandType::ADD_ALLOWED_COLLISION:
    {
      // Done
      const auto& cmd = static_cast<const tesseract_environment::AddAllowedCollisionCommand&>(command);

      visualization_->addAllowedCollision(cmd.getLinkName1(), cmd.getLinkName2(), cmd.getReason());
      break;
    }
    case tesseract_environment::CommandType::REMOVE_ALLOWED_COLLISION:
    {
      // Done
      const auto& cmd = static_cast<const tesseract_environment::RemoveAllowedCollisionCommand&>(command);
      visualization_->removeAllowedCollision(cmd.getLinkName1(), cmd.getLinkName2());
      break;
    }
    case tesseract_environment::CommandType::REMOVE_ALLOWED_COLLISION_LINK:
    {
      // Done
      const auto& cmd = static_cast<const tesseract_environment::RemoveAllowedCollisionLinkCommand&>(command);
      visualization_->removeAllowedCollision(cmd.getLinkName());
      break;
    }
    case tesseract_environment::CommandType::CHANGE_JOINT_POSITION_LIMITS:
    {
      // Done
      // const auto& cmd = static_cast<const tesseract_environment::ChangeJointPositionLimitsCommand&>(command);
      break;
    }
    case tesseract_environment::CommandType::CHANGE_JOINT_VELOCITY_LIMITS:
    {
      // Done
      // const auto& cmd = static_cast<const tesseract_environment::ChangeJointVelocityLimitsCommand&>(command);
      break;
    }
    case tesseract_environment::CommandType::CHANGE_JOINT_ACCELERATION_LIMITS:
    {
      // Done
      // const auto& cmd = static_cast<const tesseract_environment::ChangeJointAccelerationLimitsCommand&>(command);
      break;
    }
    case tesseract_environment::CommandType::ADD_KINEMATICS_INFORMATION:
    case tesseract_environment::CommandType::CHANGE_COLLISION_MARGINS:
    {
      break;
    }
  }
  return true;
}

/*
void EnvironmentWidget::modifyEnvironmentCallback(
  tesseract_msgs::srv::ModifyEnvironment::Request::SharedPtr req,
  tesseract_msgs::srv::ModifyEnvironment::Response::SharedPtr res)
{
  if (!tesseract_->getEnvironment() || req->id != tesseract_->getEnvironment()->getName() ||
      req->revision != tesseract_->getEnvironment()->getRevision())
    return false;

  return applyEnvironmentCommands(req->commands);
}
*/

/*
void EnvironmentWidget::getEnvironmentChangesCallback(
  tesseract_msgs::srv::GetEnvironmentChanges::Request::SharedPtr req,
  tesseract_msgs::srv::GetEnvironmentChanges::Response::SharedPtr res)
{
  if (req->revision > tesseract_->getEnvironment()->getRevision())
  {
    res->success = false;
    return false;
  }

  res->id = tesseract_->getEnvironment()->getName();
  res->revision = tesseract_->getEnvironment()->getRevision();
  const tesseract_environment::Commands& commands = tesseract_->getEnvironment()->getCommandHistory();
  for (int i = (req->revision - 1); i < commands.size(); ++i)
  {
    tesseract_msgs::msg::EnvironmentCommand command_msg;
    if (!tesseract_rosutils::toMsg(command_msg, *(commands[i])))
    {
      res->success = false;
      return false;
    }

    res->commands.push_back(command_msg);
  }

  res->success = true;
  return res->success;
}
*/

// void EnvironmentStateDisplay::setLinkColor(const tesseract_msgs::msg::EnvironmentState::_object_colors_type& link_colors)
//{
//  assert(false);
//  for (tesseract_msgs::EnvironmentState::_object_colors_type::const_iterator it = link_colors.begin();
//       it != link_colors.end();
//       ++it)
//  {
//    setLinkColor(it->name,
//                 QColor(static_cast<int>(it->visual[0].r * 255),
//                        static_cast<int>(it->visual[0].g * 255),
//                        static_cast<int>(it->visual[0].b * 255)));
//  }
//}

// void EnvironmentStateDisplay::setLinkColor(const std::string& link_name, const QColor& color)
//{
//  setLinkColor(&state_->getRobot(), link_name, color);
//}

// void EnvironmentStateDisplay::unsetLinkColor(const std::string& link_name)
//{
//  unsetLinkColor(&state_->getRobot(), link_name);
//}

// void EnvironmentStateDisplay::setLinkColor(Robot* robot, const std::string& link_name, const QColor& color)
//{
//  RobotLink* link = robot->getLink(link_name);

//  // Check if link exists
//  if (link)
//    link->setColor(
//        static_cast<float>(color.redF()), static_cast<float>(color.greenF()), static_cast<float>(color.blueF()));
//}

// void EnvironmentStateDisplay::unsetLinkColor(Robot* robot, const std::string& link_name)
//{
//  RobotLink* link = robot->getLink(link_name);

//  // Check if link exists
//  if (link)
//    link->unsetColor();
//}

void EnvironmentWidget::loadEnvironment()
{
  load_tesseract_ = false;

  env_->clear();
  visualization_->clear();
  if (display_mode_property_->getOptionInt() == 0)
  {
    // Initial setup
    std::string urdf_xml_string, srdf_xml_string;
    node_->get_parameter(urdf_description_property_->getString().toStdString(), urdf_xml_string);
    node_->get_parameter(urdf_description_property_->getString().toStdString() + "_semantic", srdf_xml_string);

    // Load URDF model
    if (urdf_xml_string.empty())
    {
      load_tesseract_ = true;
      RCLCPP_ERROR(node_->get_logger().get_child("EnvironmentState"), "URDF parameter empty!");
      //setStatus(rviz_common::properties::StatusProperty::Error, "EnvironmentState", "No URDF model loaded");
    }
    else if (srdf_xml_string.empty())
    {
      load_tesseract_ = true;
      RCLCPP_ERROR(node_->get_logger().get_child("EnvironmentState"), "SRDF parameter empty!");
      //setStatus(rviz_common::properties::StatusProperty::Error, "EnvironmentState", "No SRDF model loaded");
    }
    else
    {
      auto locator = std::make_shared<tesseract_rosutils::ROSResourceLocator>();
      if (env_->init(urdf_xml_string, srdf_xml_string, locator))
      {
        if (monitor_ != nullptr)
          monitor_->shutdown();

        monitor_ = std::make_unique<tesseract_monitoring::EnvironmentMonitor>(node_, env_, widget_ns_);
        revision_ = env_->getRevision();
        //setStatus(rviz_common::properties::StatusProperty::Ok, "Tesseract", "Tesseract Environment Loaded Successfully");
      }
      else
      {
        load_tesseract_ = true;
        //setStatus(rviz_common::properties::StatusProperty::Error, "Tesseract", "URDF file failed to parse");
      }
    }
  }
  else if (display_mode_property_->getOptionInt() == 1)
  {
    if (monitor_ != nullptr)
      monitor_->shutdown();

    monitor_ = std::make_unique<tesseract_monitoring::EnvironmentMonitor>(node_, env_, widget_ns_);
    if (env_->isInitialized())
      revision_ = env_->getRevision();
    else
      revision_ = 0;

    /*
    if (monitor_ != nullptr)
      monitor_->startMonitoringEnvironment(display_mode_string_property_->getStdString());
    */
  }

  if (load_tesseract_ == false && env_->isInitialized())
  {
    visualization_->addSceneGraph(*(env_->getSceneGraph()));
    visualization_->update();
    bool oldState = root_link_name_property_->blockSignals(true);
    root_link_name_property_->setStdString(env_->getRootLinkName());
    root_link_name_property_->blockSignals(oldState);
    update_required_ = true;

    changedEnableVisualVisible();
    changedEnableCollisionVisible();
    visualization_->setVisible(true);
  }

  highlights_.clear();
}

/**
 * Declare the URDF and SRDF parameters for later accessing. Does nothing if the parameters already
 * exist (i.e., have been declared elsewhere). Defaults them to empty strings if they weren't set on
 * the node.
 */
void declareDescriptionParameters(rclcpp::Node::SharedPtr node, const std::string& desc_param)
{
  if (! node->has_parameter(desc_param))
  {
    node->declare_parameter(desc_param, "");
  }

  std::string srdf_param = desc_param + "_semantic";
  if (! node->has_parameter(srdf_param))
  {
    node->declare_parameter(srdf_param, "");
  }
}

int EnvironmentWidget::environment_widget_counter_ = -1;

}  // namespace tesseract_rviz

<?php declare(strict_types=1);

namespace LORIS\issue_tracker;

use LORIS\issue_tracker\Provisioners\AttachmentProvisioner;
use \Psr\Http\Message\ServerRequestInterface;
use \Psr\Http\Message\ResponseInterface;
use \LORIS\Middleware\ETagCalculator;

/**
 * Edit class - GET & POST request methods.
 *
 * Handles issue edits and returns data in response to a front end call.
 *
 * @category Loris
 * @package  Issue_Tracker
 * @author   Caitrin Armstrong <caitrin.mcin@gmail.com>
 *           Alizée Wickenheiser <alizee.wickenheiser@mcin.ca>
 * @license  http://www.gnu.org/licenses/gpl-3.0.txt GPLv3
 * @link     https://www.github.com/aces/Loris/
 */
class Edit extends \NDB_Page implements ETagCalculator
{
    /**
     * This function will return a json response.
     *
     * @param ServerRequestInterface $request The incoming PSR7 request
     *
     * @return ResponseInterface The outgoing PSR7 response
     */
    public function handle(ServerRequestInterface $request) : ResponseInterface
    {
        // Ensure GET or POST request.
        switch ($request->getMethod()) {
        case 'GET':
            return $this->_handleGET($request);
        case 'POST':
            return $this->_handlePOST($request);
        default:
            return new \LORIS\Http\Response\JSON\MethodNotAllowed(
                $this->allowedMethods()
            );
        }
    }

    /**
     * _handleGET retrieve the edit issue details
     *
     * @param ServerRequestInterface $request The incoming PSR7 request.
     *
     * @return ResponseInterface The outgoing PSR7 response
     */
    private function _handleGET(ServerRequestInterface $request) : ResponseInterface
    {
        // Parse GET query params.
        $values = $request->getQueryParams();

        $user = $request->getAttribute('user');
        $db   = $this->loris->getDatabaseConnection();

        // get field options
        $sites = Issue_Tracker::getSites(false, true);

        //not yet ideal permissions
        $assignees      = [];
        $inactive_users = [];
        if ($user->hasPermission('access_all_profiles')) {
            $assignee_expanded = $db->pselect(
                "SELECT Real_name, UserID FROM users",
                []
            );

            $inactive_users_expanded = $db->pselect(
                "SELECT Real_name, UserID FROM users
                  WHERE Active='N'",
                []
            );
        } else {
            $CenterID = implode(',', $user->getCenterIDs());
            $DCCID    = $db->pselectOne(
                "SELECT CenterID from psc where Name='DCC'",
                []
            );
            $assignee_expanded = $db->pselect(
                "SELECT DISTINCT u.Real_name, u.UserID FROM users u
                 LEFT JOIN user_psc_rel upr ON (upr.UserID=u.ID)
                 WHERE FIND_IN_SET(upr.CenterID,:CenterID) OR (upr.CenterID=:DCC)",
                [
                    'CenterID' => $CenterID,
                    'DCC'      => $DCCID,
                ]
            );

            $inactive_users_expanded = $db->pselect(
                "SELECT DISTINCT u.Real_name, u.UserID FROM users u
                 LEFT JOIN user_psc_rel upr ON (upr.UserID=u.ID)
                 WHERE FIND_IN_SET(upr.CenterID,:CenterID) OR (upr.CenterID=:DCC)
                 AND Active='N'",
                [
                    'CenterID' => $CenterID,
                    'DCC'      => $DCCID,
                ]
            );
        }

        foreach ($assignee_expanded as $a_row) {
            $assignees[$a_row['UserID']]
                = $this->formatUserInformation($a_row['UserID']);
        }

        foreach ($inactive_users_expanded as $u_row) {
            $inactive_users[$u_row['UserID']]
                = $this->formatUserInformation($u_row['UserID']);
        }
        if (empty($inactive_users)) {
            // Force an empty array to be parsed
            // as an JSON object
            $inactive_users = new \stdClass;
        }

        $otherWatchers = [];
        $potential_watchers_expanded = $db->pselect(
            "SELECT Real_name, UserID FROM users",
            []
        );
        foreach ($potential_watchers_expanded as $w_row) {
            if ($w_row['UserID'] != $user->getUsername()) {
                $otherWatchers[$w_row['UserID']]
                    = $this->formatUserInformation($w_row['UserID']);
            }
        }

        // can't set to closed if not developer.
        if ($user->hasPermission('issue_tracker_developer')) {
            $statuses = [
                'new'          => 'New',
                'acknowledged' => 'Acknowledged',
                'assigned'     => 'Assigned',
                'feedback'     => 'Feedback',
                'resolved'     => 'Resolved',
                'closed'       => 'Closed',
            ];
        } else {
            $statuses = [
                'new'          => 'New',
                'acknowledged' => 'Acknowledged',
                'assigned'     => 'Assigned',
                'feedback'     => 'Feedback',
                'resolved'     => 'Resolved',
            ];
        }

        $priorities = [
            'low'       => 'Low',
            'normal'    => 'Normal',
            'high'      => 'High',
            'urgent'    => 'Urgent',
            'immediate' => 'Immediate',
        ];

        $unorgCategories = $db->pselect(
            "SELECT categoryName FROM issues_categories",
            []
        );
        $categories      = [];
        foreach ($unorgCategories as $r_row) {
            $categoryName = $r_row['categoryName'];
            if ($categoryName) {
                $categories[$categoryName] = $categoryName;
            }
        }

        $allModules = \Module::getActiveModulesIndexed($this->loris);

        $modules = [];
        foreach ($allModules as $key => $m) {
            $modules[$key] = $m->getLongName();
        }

        // Now get issue values
        $issueData = $this->getIssueData(null, $user);
        if (!empty($values['issueID'])
            && $values['issueID'] != "new"
        ) { // if an existing issue
            $issueID   = intval($values['issueID']);
            $issueData = $this->getIssueData($issueID, $user);

            $desc = $db->pselect(
                "SELECT issueComment
                FROM issues_comments WHERE issueID=:i
                ORDER BY dateAdded LIMIT 1",
                ['i' => $issueID]
            );

            $provisioner = (new AttachmentProvisioner($issueID));
            $attachments = (new \LORIS\Data\Table())
                ->withDataFrom($provisioner)
                ->toArray($user);

            $isWatching            = $db->pselectOne(
                "SELECT userID, issueID FROM issues_watching
                WHERE issueID=:issueID AND userID=:userID",
                [
                    'issueID' => $issueID,
                    'userID'  => $user->getUsername(),
                ]
            );
            $issueData['watching'] = $isWatching === null ? 'No' : 'Yes';
            $issueData['commentHistory'] = $this->getComments($issueID);
            $issueData['attachments']    = $attachments;
            $issueData['whoami']         = $user->getUsername();
            $issueData['othersWatching'] = $this->getWatching($issueID);

            // We need to unescape the string here:
            // React is escaping the string in the template
            // This fixes an issue with multiple escaping (#6643)
            $issueData['desc'] = '';
            if (count($desc) > 0) {
                $comment           = $desc[0]['issueComment'];
                $issueData['desc'] = html_entity_decode($comment);
            }
        }
        $issueData['comment'] = null;

        $isOwnIssue = $issueData['reporter'] == $user->getUsername();

        $result = [
            'assignees'         => $assignees,
            'inactiveUsers'     => $inactive_users,
            'sites'             => $sites,
            'statuses'          => $statuses,
            'priorities'        => $priorities,
            'categories'        => $categories,
            'modules'           => $modules,
            'otherWatchers'     => $otherWatchers,
            'issueData'         => $issueData,
            'hasEditPermission' => $user->hasPermission(
                'issue_tracker_developer'
            ),
            'isOwnIssue'        => $isOwnIssue,
        ];

        return new \LORIS\Http\Response\JsonResponse(
            $result
        );
    }

    /**
     * If issueID is passed retrieves issue data from database,
     * otherwise return empty issue data object
     *
     * @param int|null $issueID the ID of the requested issue
     * @param \User    $user    the user requesting the issue
     *
     * @return array
     */
    function getIssueData(int $issueID=null, \User $user): array
    {
        $db = $this->loris->getDatabaseConnection();

        if (!empty($issueID)) {
            $issueData = $db->pselectRow(
                "SELECT
                            i.*,
                            c.PSCID,
                            s.Visit_label AS visitLabel
                        FROM
                            issues AS i
                            LEFT JOIN candidate c ON (i.candID = c.CandID)
                            LEFT JOIN session s ON (i.sessionID = s.ID)
                        WHERE
                            issueID = :issueID",
                ['issueID' => $issueID]
            ) ?? [];
            // format users
            $issueData['reporter']      = $this->formatUserInformation(
                $issueData['reporter']
            );
            $issueData['assignee']      = $issueData['assignee'] ?
                $this->formatUserInformation(
                    $issueData['assignee']
                ) :
                null;
            $issueData['lastUpdatedBy'] = $this->formatUserInformation(
                $issueData['lastUpdatedBy']
            );
            // return
            return $issueData;
        }

        return [
            'reporter'      => $this->formatUserInformation($user->getUsername()),
            'dateCreated'   => date('Y-m-d H:i:s'),
            'centerID'      => $user->getCenterIDs(),
            'status'        => 'new',
            'priority'      => 'normal',
            'issueID'       => 0, // TODO: this is dumb
            'title'         => null,
            'lastUpdate'    => null,
            'PSCID'         => null,
            'assignee'      => null,
            'history'       => null,
            'watching'      => "Yes",
            'visitLabel'    => null,
            'category'      => null,
            'lastUpdatedBy' => null,
        ];
    }

    /**
     * Gets the changes to values, and the comments relevant to the given issue
     *
     * @param int $issueID the issueID
     *
     * @return array $commentHistory
     */
    function getComments(int $issueID): array
    {
        $db = $this->loris->getDatabaseConnection();

        $unformattedComments = $db->pselect(
            "SELECT newValue, fieldChanged, dateAdded, addedBy " .
            "FROM issues_history where issueID=:issueID " .
            "UNION " .
            "SELECT issueComment, 'comment', dateAdded, addedBy " .
            "FROM issues_comments where issueID=:issueID ",
            ['issueID' => $issueID]
        );

        // looping by reference so can edit in place
        $modules = \Module::getActiveModulesIndexed($this->loris);
        foreach ($unformattedComments as &$comment) {
            // for all users that added a comment, get their full name
            $comment['addedBy'] = $this->formatUserInformation($comment['addedBy']);
            // specific changes
            if ($comment['fieldChanged'] === 'module') {
                $mid = $comment['newValue'];
                if (array_key_exists($mid, $modules)) {
                    $comment['newValue'] = $modules[$mid]->getLongName();
                } else {
                    $comment['newValue'] = "Linked module(s) do not exist";
                }
            } else if ($comment['fieldChanged'] === 'centerID') {
                $comment['newValue']     = $this->getSiteName(
                    intval($comment['newValue'])
                );
                $comment['fieldChanged'] = 'site';
            } else if ($comment['fieldChanged'] === 'candID') {
                $PSCID = $db->pselectOne(
                    "SELECT PSCID FROM candidate WHERE CandID=:candID",
                    ['candID' => $comment['newValue']]
                );
                $comment['newValue']     = $PSCID;
                $comment['fieldChanged'] = 'PSCID';
            } else if ($comment['fieldChanged'] === 'sessionID') {
                $visitLabel          = $db->pselectOne(
                    "SELECT Visit_label FROM session WHERE ID=:sessionID",
                    ['sessionID' => $comment['newValue']]
                );
                $comment['newValue'] = $visitLabel;
                $comment['fieldChanged'] = 'Visit Label';
            } else if ($comment['fieldChanged'] === 'assignee'
                || $comment['fieldChanged'] === 'lastUpdatedBy'
            ) {
                $comment['newValue']
                    = $this->formatUserInformation($comment['newValue']);
            }
        }
        return $unformattedComments; //now formatted I guess
    }

    /**
     * Gets a list of those watching an issue.
     *
     * @param int $issueID the relevant issue
     *
     * @return array those who are watching
     */
    function getWatching(int $issueID): array
    {
        $db = $this->loris->getDatabaseConnection();

        $watching = $db->pselect(
            "SELECT userID from issues_watching WHERE issueID=:issueID",
            ['issueID' => $issueID]
        );

        $whoIsWatching = [];
        foreach ($watching as $watcher) {
            $whoIsWatching[] = $watcher['userID'];
        }
        return $whoIsWatching;
    }

    /**
     * Returns the site name in the database corresponding to the centerID
     *
     * @param int $centerID - must match one from the psc table or 0
     *
     * @return ?string
     */
    function getSiteName(int $centerID): ?string
    {
        return $centerID == null
            ? 'All Sites'
            : \Site::singleton(new \CenterID(strval($centerID)))->getCenterName();
    }

    /**
     * Processes the values & saves to database and return a json response.
     *
     * @param ServerRequestInterface $request The incoming PSR7 request.
     *
     * @return ResponseInterface The outgoing PSR7 response
     */
    private function _handlePOST(ServerRequestInterface $request) : ResponseInterface
    {
        // Parse POST request body.
        $values = $request->getParsedBody();
        if (!is_array($values)) {
            return new \LORIS\Http\Response\JSON\BadRequest(
                'Invalid request'
            );
        }

        $user = $request->getAttribute('user');
        $db   = $this->loris->getDatabaseConnection();

        $issueValues    = [];
        $validateValues = [];
        $fields         = [
            'assignee',
            'status',
            'priority',
            'centerID',
            'title',
            'category',
            'module',
        ];
        $fieldsToValidateFirst = [
            'PSCID',
            'visitLabel',
            'centerID',
        ];

        foreach ($fields as $field) {
            // The default is a string "null" because if the front end submits
            // null, it comes through as a string, so the default is to behave
            // the same way
            $value = $values[$field] ?? "null";
            if ($value === "null") {
                $value = null;
            }
            if (isset($field)) {
                $issueValues[$field] = $value;
            }
        }
        foreach ($fieldsToValidateFirst as $vField) {
            if (isset($values[$vField]) && $values[$vField] !== "null") {
                $validateValues[$vField] = $values[$vField];
            }
        }

        $issueID = intval($values['issueID']);

        $issueValues['lastUpdatedBy'] = $user->getUserName();

        $validatedInput = $this->validateInput($validateValues, $user);
        if (!is_array($validatedInput)) { // Error exists.
            return $validatedInput;
        }
        if (array_key_exists('sessionID', $validatedInput)) {
            $issueValues['sessionID'] = $validatedInput['sessionID'];
        }

        if (array_key_exists('candID', $validatedInput)) {
            $issueValues['candID'] = $validatedInput['candID'];
        }

        // Get changed values to save in history
        $issueValuesWithWatching = $issueValues;
        if (isset($values['othersWatching'])) {
            $usersWatching = $values['othersWatching'];
            $issueValuesWithWatching['watching'] = $usersWatching;
        }
        $historyValues = $this->getChangedValues(
            $issueValuesWithWatching,
            $issueID,
            $user
        );

        if (!empty($issueID)) {
            $db->update('issues', $issueValues, ['issueID' => $issueID]);
        } else {
            $issueValues['reporter']    = $user->getUsername();
            $issueValues['dateCreated'] = date('Y-m-d H:i:s');
            $db->insert('issues', $issueValues);
            $issueID = intval($db->getLastInsertId());
        }

        // convert the aspect of 'lastUpdatedBy' to Fullname
        // once the db has registered the username (userID)
        $issueValues['lastUpdatedBy']
            = $this->formatUserInformation($user->getUserName());

        $this->updateHistory($historyValues, $issueID, $user);
        $this->updateComments($values['comment'], $issueID, $user);

        // Attachment for new issue.
        $uploadedFiles = $request->getUploadedFiles();
        if (!empty($uploadedFiles)) {
            $attachment = new \LORIS\issue_tracker\UploadHelper();
            $success    = $attachment->setupUploading(
                $user,
                $uploadedFiles,
                [
                    'fileDescription' => '',
                    'issueID'         => $issueID,
                ]
            );
            if (!$success) {
                $this->logger->log(
                    \Psr\Log\LogLevel::ERROR,
                    $attachment->errorMessage
                );
                return new \LORIS\Http\Response\JSON\Conflict(
                    $attachment->errorMessage
                );
            }
        }

        // Adding new assignee to watching
        if (isset($issueValues['assignee'])) {
            $nowWatching = [
                'userID'  => $issueValues['assignee'],
                'issueID' => $issueID,
            ];
            $db->replace('issues_watching', $nowWatching);

            // sending email
            $this->emailUser($issueID, $issueValues['assignee'], '', $user);
        }

        // Adding others from multiselect to watching table.
        if (isset($values['othersWatching'])) {

            // Clear the list of current watchers
            $db->delete(
                'issues_watching',
                ['issueID' => $issueID]
            );

            // Add new watchers (if any)
            $usersWatching = explode(',', $values['othersWatching']);
            foreach ($usersWatching as $usersWatching) {
                if ($usersWatching) {
                    $db->insert(
                        'issues_watching',
                        [
                            'userID'  => $usersWatching,
                            'issueID' => $issueID,
                        ]
                    );
                    // Send email to watchers if they are not the assignee
                    if (isset($issueValues['assignee'])
                        && $issueValues['assignee'] == $usersWatching
                    ) {
                        continue;
                    }
                    $this->emailUser($issueID, '', $usersWatching, $user);
                }
            }
        }

        // Add editor to the watching table unless they don't want to be added.
        if (isset($values['watching']) && $values['watching'] == 'Yes') {
            $nowWatching = [
                'userID'  => $user->getUsername(),
                'issueID' => $issueID,
            ];
            $db->replace('issues_watching', $nowWatching);
        } else if (isset($values['watching']) && $values['watching'] == 'No') {
            $db->delete(
                'issues_watching',
                [
                    'issueID' => $issueID,
                    'userID'  => $user->getUsername(),
                ]
            );
        }
        return new \LORIS\Http\Response\JsonResponse(
            ['issueID' => $issueID]
        );
    }

    /**
     * Emails all users that are watching the issue with the changes.
     *
     * @param int    $issueID          the issueID
     * @param string $changed_assignee changed assignee
     * @param string $changed_watcher  changed watcher
     * @param \User  $user             the user requesting the change
     *
     * @return void
     */
    function emailUser(int $issueID, string $changed_assignee,
        string $changed_watcher, \User $user
    ) {
        $db      = $this->loris->getDatabaseConnection();
        $baseurl = \NDB_Factory::singleton()->settings()->getBaseURL();

        $title = $db->pSelectOne(
            "SELECT title FROM issues
            WHERE issueID=:issueID",
            ['issueID' => $issueID]
        );

        $msg_data            = [];
        $msg_data['url']     = $baseurl .
            "/issue_tracker/issue/" . $issueID;
        $msg_data['issueID'] = $issueID;
        $msg_data['currentUser'] = $user->getUsername();
        $msg_data['title']       = $title;

        if (isset($changed_assignee)) {
            $issueChangeEmailsAssignee = $db->pselect(
                "SELECT
                            u.Email AS Email,
                            u.First_name AS firstname
                        FROM
                            users u
                        WHERE
                            u.UserID = :assignee
                            AND u.UserID != :currentUser",
                [
                    'assignee'    => $changed_assignee,
                    'currentUser' => $user->getUserName(),
                ]
            );

            if (isset($issueChangeEmailsAssignee[0])) {
                $msg_data['firstname'] = $issueChangeEmailsAssignee[0]['firstname'];

                \Email::send(
                    $issueChangeEmailsAssignee[0]['Email'],
                    'issue_assigned.tpl',
                    $msg_data
                );
            }
        }

        $issue_change_emails = $db->pselect(
            "SELECT
                        u.Email AS Email,
                        u.First_name AS firstname
                    FROM
                        users u
                        INNER JOIN issues_watching w ON (w.userID = u.UserID)
                    WHERE
                        w.issueID = :issueID
                        AND u.UserID = :watcher
                        AND u.UserID != :currentUser",
            [
                'issueID'     => $issueID,
                'watcher'     => $changed_watcher,
                'currentUser' => $user->getUserName(),
            ]
        );

        $msg_data['url']         = $baseurl .
            "/issue_tracker/issue/" . $issueID;
        $msg_data['issueID']     = $issueID;
        $msg_data['currentUser'] = $user->getUsername();

        foreach ($issue_change_emails as $email) {
            $msg_data['firstname'] = $email['firstname'];
            \Email::send($email['Email'], 'issue_change.tpl', $msg_data);
        }
    }

    /**
     * Validate PSCID and Visit Label
     *
     * @param array $values values to be validated
     * @param \User $user   the user requesting the change
     *
     * @return \LORIS\Http\Response\JSON\Conflict|array
     */
    function validateInput(array $values, \User $user)
    {
        $db         = $this->loris->getDatabaseConnection();
        $pscid      = ($values['PSCID'] ?? null);
        $visitLabel = ($values['visitLabel'] ?? null);
        $centerID   = ($values['centerID'] ?? null);
        $result     = [
            'PSCID'             => $pscid,
            'visit'             => $visitLabel,
            'centerID'          => $centerID,
            'candID'            => null,
            'sessionID'         => null,
            'isValidSubmission' => true,
            'invalidMessage'    => null,
        ];

        if (isset($result['PSCID'], $result['centerID'])) {
            $validCenter = $db->pselectOne(
                "
                SELECT
                    RegistrationCenterID = :center_id as CenterID
                FROM
                    candidate
                WHERE
                    PSCID = :psc_id
            ",
                [
                    "center_id" => $result['centerID'],
                    "psc_id"    => $result['PSCID'],
                ]
            );
            if (!$validCenter) {
                $validCenter = $db->pselectOne(
                    "
                    SELECT
                        EXISTS (
                            SELECT
                                *
                            FROM
                                session s
                            JOIN
                                candidate c
                            ON
                                c.CandID = s.CandID
                            WHERE
                                s.CenterID = :center_id AND
                                c.PSCID = :psc_id
                        )
                ",
                    [
                        "center_id" => $result['centerID'],
                        "psc_id"    => $result['PSCID'],
                    ]
                );
            }
            if (!$validCenter) {
                $this->logger->log(
                    \Psr\Log\LogLevel::WARNING,
                    'PSCID and Center ID do not match a valid session!'
                );
                return new \LORIS\Http\Response\JSON\Conflict(
                    'PSCID and Center ID do not match a valid session!'
                );
            }
        }
        // If both are set, return SessionID and CandID
        if (isset($result['PSCID']) && isset($result['visit'])) {
            $session = $db->pSelect(
                "SELECT s.ID as sessionID, c.candID as candID FROM candidate c
                INNER JOIN session s on (c.CandID = s.CandID)
                WHERE c.PSCID=:PSCID and s.Visit_label=:visitLabel",
                [
                    'PSCID'      => $result['PSCID'],
                    'visitLabel' => $result['visit'],
                ]
            );

            if (isset($session[0]['sessionID'])) {
                $result['sessionID'] = $session[0]['sessionID'];
                $result['candID']    = $session[0]['candID'];
            } else {
                $this->logger->log(
                    \Psr\Log\LogLevel::ERROR,
                    'PSCID and Visit Label do not match a valid candidate session!'
                );
                return new \LORIS\Http\Response\JSON\Conflict(
                    'PSCID and Visit Label do not match a valid candidate session!'
                );
            }
            return $result;
        }

        // If only PSCID is set, return CandID
        if (isset($result['PSCID'])) {
            $query  = "SELECT CandID FROM candidate WHERE PSCID=:PSCID";
            $params = ['PSCID' => $result['PSCID']];

            if (!$user->hasPermission('access_all_profiles')) {
                $params['CenterID'] = implode(',', $user->getCenterIDs());
                $query .= " AND FIND_IN_SET(RegistrationCenterID,:CenterID)";
            }

            $candidate = $db->pSelectOne($query, $params);
            if ($candidate) {
                $result['candID'] = $candidate;
            } else {
                $this->logger->log(
                    \Psr\Log\LogLevel::ERROR,
                    'PSCID does not match a valid candidate!'
                );
                return new \LORIS\Http\Response\JSON\Conflict(
                    'PSCID does not match a valid candidate!'
                );
            }

            return $result;
        }

        // If only visit label is set, return an error
        if (isset($result['visit'])) {
            $this->logger->log(
                \Psr\Log\LogLevel::ERROR,
                'Visit Label must be accompanied by a PSCID'
            );
            return new \LORIS\Http\Response\JSON\Conflict(
                'Visit Label must be accompanied by a PSCID'
            );
        }
        return $result;
    }

    /**
     * Iterates through submitted values and filters only values that have changed
     *
     * @param array $issueValues new values
     * @param int   $issueID     issue ID
     * @param \User $user        current user
     *
     * @return array array of changed values
     */
    function getChangedValues(array $issueValues, int $issueID, \User $user): array
    {
        $issueData     = $this->getIssueData($issueID, $user);
        $changedValues = [];
        foreach ($issueValues as $key => $value) {
            // Only include fields that have changed
            // centerID is allowed to be NULL
            if ($issueValues[$key] != ($issueData[$key] ?? '')
                && (!empty($value) || $key === 'centerID')
            ) {
                $changedValues[$key] = $value;
            }
        }
        return $changedValues;
    }

    /**
     * Get a formatted string from user information.
     *
     * @param string $userid a username/userid
     *
     * @return string a formatted string "fullname (userid)"
     */
    function formatUserInformation(string $userid)
    {
        $user = \User::factory($userid);
        $un   = $user->getUsername();
        if (empty($un)) {
            // e.g. in case of anonymous user
            return $user->getFullname();
        } else {
            return $user->getFullname() . " (" . $un . ")";
        }
    }

    /**
     * Puts updated fields into the issues_history table.
     *
     * @param array $values  the new values
     * @param int   $issueID the issue ID
     * @param \User $user    the user
     *
     * @return void
     */
    function updateHistory(array $values, int $issueID, \User $user)
    {
        $db = $this->loris->getDatabaseConnection();
        foreach ($values as $key => $value) {
            // centerID is allowed to be NULL
            if (!empty($value) || $key === 'centerID') {
                $changedValues = [
                    'newValue'     => $value ?? '',
                    'fieldChanged' => $key,
                    'issueID'      => $issueID,
                    'addedBy'      => $user->getUsername(),
                ];
                $db->insert('issues_history', $changedValues);
            }
        }
    }

    /**
     * Puts updated fields into the issues_comments table.
     *
     * @param string $comment new issue comment
     * @param int    $issueID the issue ID
     * @param \User  $user    the user
     *
     * @return void
     */
    function updateComments(string $comment, int $issueID, \User $user)
    {
        $db = $this->loris->getDatabaseConnection();
        if (isset($comment) && $comment != "null") {
            $commentValues = [
                'issueComment' => $comment,
                'addedBy'      => $user->getUsername(),
                'issueID'      => $issueID,
            ];
            $db->insert('issues_comments', $commentValues);
        }
    }

    /**
     * Returns true if the user has permission to access
     * the issue_tracker module
     *
     * @param \User $user The user whose access is being checked
     *
     * @return bool true if user has permission
     */
    function _hasAccess(\User $user) : bool
    {
        return $user->hasPermission('issue_tracker_reporter')
            || $user->hasPermission('issue_tracker_developer');
    }

    /**
     * An ETagCalculator provides the ability to calculate an ETag for
     * an incoming HTTP request.
     *
     * @param ServerRequestInterface $request The incoming PSR7 request.
     *
     * @return string The value to use for the ETag header.
     */
    public function ETag(ServerRequestInterface $request): string
    {
        if ($request->getMethod() === 'POST') {
            return '';
        }
        return md5(json_encode((string) $this->_handleGET($request)->getBody()));
    }

    /**
     * Return an array of valid HTTP methods for this endpoint
     *
     * @return string[] Valid versions
     */
    protected function allowedMethods(): array
    {
        return [
            'GET',
            'POST',
        ];
    }
}
